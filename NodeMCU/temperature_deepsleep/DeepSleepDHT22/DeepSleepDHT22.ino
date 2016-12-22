#include <ESP8266WiFi.h>

#include <WiFiClient.h>
#include "DallasTemperature.h"
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"        //https://github.com/tzapu/WiFiManager

const int PIN_SENSOR_DS18B20 = 4;     //GPIO4 = DS18B20 one-wire signal
const int PIN_LED = 2; //On the ESP-12, the onboard LED is connected to pin 2

/* 
 *  DS18B20 temperature sensore
 */
OneWire oneWire(PIN_SENSOR_DS18B20);
DallasTemperature DS18B20(&oneWire);
float valueTemperatureDS18B20 = 0.0;

void updateSensorDS18B20(void) {
  DS18B20.requestTemperatures(); 
  valueTemperatureDS18B20 = DS18B20.getTempCByIndex(0);
}



#define DEBUGPRINT

#ifdef DEBUGPRINT
#define DEBUG_PRINT(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#endif


//#include <SimpleTimer.h>          //http://playground.arduino.cc/Code/SimpleTimer  || https://github.com/jfturcot/SimpleTimer

char* topic = "nodes/battery/temp";
char* hellotopic = "nodes/register";
//IPAddress server(192, 168, 10, 10);

//SimpleTimer timer;

String clientName;
WiFiClient wifiClient;

//void callback(const MQTT::Publish& pub) {
// handle message arrived
//}

//PubSubClient client(wifiClient, "iot.eclipse.org");

//emoncoms
const char* host = "emoncms.org";
const char* nodeId   = "21";

//either define key here of create a config.h key and add it there
//const char* privateKey = "aa017f2197ad2e2562731a42ad1f9c4f";
#import "config.h"

ADC_MODE(ADC_VCC);


float h = 0;
float t = 0;
int vcc;
bool valueDiscrete = 1;

long startMills;


void sendData();

void setup(void)
{
  //ESP.eraseConfig();

  startMills = millis();
  // start serial port
  Serial.begin(115200);

  //-------------------

  pinMode(PIN_LED, OUTPUT);       // Initialize the BUILTIN_LED pin as an output
  
  DEBUG_PRINT();

  WiFiManager wifi;
 // wifi.resetSettings();
  
  wifi.setTimeout(120); //so if it restarts and router is not yet online, it keeps rebooting and retrying to connect
  
  if (!wifi.autoConnect("Nahatch ESP")) {
    DEBUG_PRINT("timeout - going to sleep");
    DEBUG_PRINT(millis() - startMills);

    delay(200);
    //sleep and try again
    ESP.deepSleep(10 * 60 * 1000 * 1000);
    delay(1000);
  }

  DEBUG_PRINT(millis() - startMills);
  //-------
 //setup hardware

}


void loop(void)
{

  vcc = ESP.getVcc();//readvdd33();
  // original loop

  DEBUG_PRINT(millis() - startMills);

  DEBUG_PRINT("Requesting temperatures...");
  int acquireresult;
  
  //read twice as the first result is cached from last time. suggested by @chaeplin
  updateSensorDS18B20();
  delay(2000);
  updateSensorDS18B20();
  DEBUG_PRINT("Temperature: ");
  DEBUG_PRINT(valueTemperatureDS18B20);

  sendData();
  DEBUG_PRINT("DONE");


  DEBUG_PRINT(millis() - startMills);
  DEBUG_PRINT("Going to sleep");
  delay(250);

  ESP.deepSleep(2 * 60 * 1000 * 1000);
  //ESP.deepSleep(60 * 1000 * 1000);
  delay(1000);
}


void sendData() {
  DEBUG_PRINT("connecting to ");
  DEBUG_PRINT(host);

  WiFiClient emoClient;

  const int httpPort = 80;
  if (!emoClient.connect(host, httpPort)) {
    DEBUG_PRINT("connection failed");
    return;
  }

  String json = "{temperature:";
  json += valueTemperatureDS18B20;
  json += ",vcc:";
  json += vcc;
  json += "}";

  String url = "/input/post.json?node=";
  url += nodeId;
  url += "&apikey=";
  url += privateKey;
  url += "&json=";
  url += json;

  DEBUG_PRINT("Requesting URL: ");
  DEBUG_PRINT(url);

  // This will send the request to the server
  emoClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "Connection: close\r\n\r\n");
  delay(10);

  // Read all the lines of the reply from server and print them to Serial
  while (emoClient.available()) {
    String line = emoClient.readStringUntil('\r');
    DEBUG_PRINT(line);
  }

  DEBUG_PRINT();
  DEBUG_PRINT("closing connection");
/*
  if (client.connected()) {
    DEBUG_PRINT("Sending mqtt: ");
    //DEBUG_PRINT(json);

    if (client.publish(topic, (char*) json.c_str())) {
      DEBUG_PRINT("Publish ok");
    }
    else {
      DEBUG_PRINT("Publish failed");
    }
  }*/


}

void toggleLED() {
  valueDiscrete = !valueDiscrete;
  digitalWrite(PIN_LED, valueDiscrete);
}


