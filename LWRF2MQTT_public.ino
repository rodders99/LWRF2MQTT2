/*
	LightwaveRF (Receiver) to MQTT 
*/

// Edit the config.h.example file and save as config.h to setup parameters
#include "config.h"

//ESP8266 / ESP32 includes for wifi
#if defined (ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#elif defined (ESP32)
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiSTA.h>
#include <WiFiType.h>
#include <WiFiUdp.h>
#endif

#include <PubSubClient.h>

#include <LwRx.h>

//setup lwrf storage for deduplication
String lwrf_rec_array[50] = {};


#if defined(ESP8266)
  String newHostname = "dev";
#elif defined(ESP32)
  const char* newHostname = "HOSTNAME";
#endif



//MQTT client
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

//lwrf constants
long last_time = 0;
char data[100];
char cDataBuffer[8];

//Repeats data
static byte repeats = 0;
static byte timeout = 20;

//pair data
static byte pairtimeout = 50;
static byte pairEnforce = 0;
static byte pairBaseOnly = 0;

//Serial message input
const byte maxvalues = 10;
byte indexQ;
boolean newvalue;
int invalues[maxvalues];
char *msgstr;
byte msglen = 10;
byte message[10] = { 0x80 , 0x15, 2 ,15 ,3 , 6, 0 ,4 ,13 };


void connectToWiFi() 
{
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.print("WIFI Connecting to ");

  WiFi.begin(SSID, PWD);
  Serial.print(SSID);

  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  //Set new hostname
  #if defined ESP8266
    WiFi.hostname(newHostname);
  #elif defined ESP32
    WiFi.setHostname(newHostname);
  #endif

  Serial.print("WiFi hostname \t\t:\t ");
  Serial.println(WiFi.hostname());
  Serial.print("WIFI Connected to \t:\t ");
  Serial.println(SSID);
  Serial.print("IP Address \t\t:\t ");
  Serial.println(WiFi.localIP());

 
}

void setupMQTT() 
{
	while (!mqttClient.connected()) 
	{
		Serial.println("MQTT connecting ...");

		mqttClient.setServer(mqttServer, mqttPort);

		if (mqttClient.connect(mqtt_id, "", "", mqtt_lwt_topic, 2, 0, "offline", 0)) 
		{
			mqttClient.publish(mqtt_lwt_topic, "online", true);
			Serial.println("MQTT connected");
      Serial.print("MQTT Publish topic \t:\t ");
      Serial.println(String(mqtt_topic));
      Serial.print("MQTT LWT Status topic \t:\t ");
      Serial.println(String(mqtt_lwt_topic));

		}
		else 
		{
			Serial.print("MQTT Connect failed, status code = ");
			Serial.print(mqttClient.state());
			Serial.println("MQTT try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

void setup() {
 	lwrx_setup(RX_PIN);
	Serial.begin(115200);
	connectToWiFi();


  setupMQTT();
//  Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(newHostname.c_str());

//  No authentication by default
  ArduinoOTA.setPassword(OTApwd);

  ArduinoOTA.onStart([]() {
	  String type;
	  if (ArduinoOTA.getCommand() == U_FLASH)
		  type = "sketch";
	  else // U_SPIFFS
		  type = "filesystem";
	  Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
	  Serial.println("\nOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
	  Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
	  Serial.printf("OTA Error[%u]: ", error);
	  if (error == OTA_AUTH_ERROR) Serial.println("OTA Auth Failed");
	  else if (error == OTA_BEGIN_ERROR) Serial.println("OTA Begin Failed");
	  else if (error == OTA_CONNECT_ERROR) Serial.println("OTA Connect Failed");
	  else if (error == OTA_RECEIVE_ERROR) Serial.println("OTA Receive Failed");
	  else if (error == OTA_END_ERROR) Serial.println("OTA End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("OTA Hostname \t\t:\t ");
  Serial.println(newHostname);

  Serial.print("OTA IP address \t\t:\t ");
  Serial.println(WiFi.localIP());
  Serial.print("OTA Password \t\t:\t ");
  Serial.println(String(OTApwd));
 }



void reconnect() {
  Serial.println("Connecting to MQTT Broker...");
  while (!mqttClient.connected()) {
    Serial.println("Reconnecting to MQTT Broker..");

    if (mqttClient.connect(mqtt_id)) {
		mqttClient.publish(mqtt_lwt_topic, "online", true);
      	Serial.println("MQTT Connected.");
      
    }

  }
}

void loop() {
	int len_s = getArrayySize();
	ArduinoOTA.handle();

	if (!mqttClient.connected())
		reconnect();

	mqttClient.loop();
	String vmsg;

	if (lwrx_message()) 
	{
		Serial.println("Got Something");
		lwrx_getmessage(message, msglen);


		char hexadecimalnum[10];
		for (byte i = 0; i < msglen; i++) {
			sprintf(hexadecimalnum, "%X", message[i]);
			vmsg = vmsg + hexadecimalnum;
		}
		Serial.println(vmsg);
		Serial.println(" ");
		int str_len = vmsg.length() + 1;
		char char_array[str_len];
		vmsg.toCharArray(char_array, str_len);
		char* token = strtok(char_array, " ");

// Change MQTT Topic as required
		mqttClient.publish("home/433toMQTTdev", token);

// Check if LWRF code previously received, if not, store it in the array and print the automation
		if (check_message_stored(vmsg, len_s) == false ) 
			{

				Serial.println("check_message_stored = false, it's a new code");
				lwrf_rec_array[len_s] = vmsg;
				//storeNewCode(vmsg);
				Serial.println("Updated Array");
				//Serial.print("NEW Array length ");
				Serial.println(len_s, DEC);
				NewHAautomation(vmsg);
				
			}
	}

	


}






bool check_message_stored(String recvd, int la) {
    
  // int la = getArrayySize();
  Serial.println("check_message_stored");
    Serial.println("================================================================");

  Serial.println("Array element \t \t In Array\t\tReceived LWRF Code");
  for (int x = 0; x < la; x++) 
  {

    Serial.print("\t");
    Serial.print(x, DEC);
    Serial.print("\t = \t ");
    Serial.print(lwrf_rec_array[x]);
    Serial.print(" \t = \t");
    Serial.print(recvd);
    if (recvd == lwrf_rec_array[x])
    {
      
        Serial.println("   = TRUE Valid Lookup, we'll do nothing");
        return true;

    } else 
    
    {
        Serial.println("   = FALSE Invalid lookup");
    }
  }
  return false;
}


int getArrayySize() 
{
  int lc = sizeof(lwrf_rec_array)/sizeof(lwrf_rec_array[0]);
  int cnt = 0 ;
  for (int x = 0; x < lc; x++) 
  {
      if(lwrf_rec_array[x] != NULL ) 
      {
      cnt = cnt + 1;
      }
  }
  return cnt;
}

void NewHAautomation(String recvd) {
  Serial.println("Automation Code script for Home Assistant");
  Serial.println("===========================================");
  Serial.println();
  Serial.println();
  Serial.print("- id: '");
  Serial.print(recvd);
  Serial.println("'");
  Serial.print("  alias: 'Button ");
  Serial.print(recvd);
  Serial.println("'");
  Serial.println("  trigger: ");
  Serial.print("  - payload: "); 
  Serial.println(recvd);
  Serial.println("    platform: mqtt");
  Serial.println("    topic: home/433toMQTT");
  Serial.println("  condition: []");
  Serial.println("  action:");
  Serial.println("  - data:");
  Serial.println("      entity_id: switch.example");
  Serial.println("      state: 'off'");
  Serial.println("    service: python_script.set_state");
}
