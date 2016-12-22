// Import required libraries
#include "Arduino.h" 
#include "ThingSpeak.h"
#include "ESP8266WiFi.h"
#include <DHT.h>
#include "DallasTemperature.h"
#include <rBase64.h>
#include <ESP8266WiFi.h>
#include "EIoTCloudRestApi.h"

// EasyIoT server definitions
// http://iot-playground.com/blog/2-uncategorised/86-easyiot-cloud-mqtt-api-v1
#define EIOT_USERNAME    "lkubecka"
#define EIOT_PASSWORD    "L1borovo"
#define EIOT_IP_ADDRESS  "176.9.90.169" // cloud.iot-playground.com
#define EIOT_PORT        1883
#define EIOT_NODE        "ESP"

#define EIOT_CLOUD_ADDRESS     "cloud.iot-playground.com"
#define EIOT_CLOUD_PORT        40404
#define EIOT_CLOUD_ID_TEMPERATURE    "58448d11c943a07ae877855b/xBjMWbeSsRHRkO72"
#define EIOT_CLOUD_ID_HUMIDITY    "58448d11c943a07ae877855b/eTRRietRikWMGiTS"
#define EIOT_CLOUD_ID_TEMPERATURE_DALLAS    "58448d11c943a07ae877855b/6IV04S5kgSBwmrGm"

const int PIN_LED = 2; //On the ESP-12, the onboard LED is connected to pin 2
const int PIN_SENSOR_DHT = 5;     //GPIO5 = AM2321 signal
const int PIN_SENSOR_DS18B20 = 4;     //GPIO4 = DS18B20 one-wire signal

unsigned long myChannelNumber = 180204;
const char * myWriteAPIKey = "TCIDZX6BSW1KD29W";
 


String webString="";     // String to display
unsigned long lastMillisSensors = 0;        // will store last temp was read, use "unsigned long" for variables that hold time

const unsigned long UPDATE_TIME_SENSORS = 10000;//ms
const unsigned long UPDATE_TIME_SERVER = 1000; //ms
const unsigned long UPDATE_TIME_MG811 = 200;//ms
const unsigned long UPDATE_TIME_LED = 250;//ms
const unsigned long UPDATE_TIME_STATUS_VPINS = 500;//ms
const unsigned long UPDATE_TIME_VALUE_VPINS = 500;//ms

boolean isFaultDHT = false;

float valueTemperatureDHT = 0.0;
float valueHumidityDHT = 0.0;
bool valueDiscrete = 0;
float valueTemperatureDS18B20 = 0.0;

int status = WL_IDLE_STATUS;
WiFiClient  client;

/*
 * DHT temperture/humidity sensor
 */

#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

DHT dht(PIN_SENSOR_DHT, DHTTYPE, 15);

void updateSensorDHT(void) {
  valueTemperatureDHT = (float)dht.readTemperature();
  valueHumidityDHT = (float)dht.readHumidity();
  isFaultDHT = (isnan(valueTemperatureDHT)) || (isnan(valueHumidityDHT));
}


/* 
 *  DS18B20 temperature sensore
 */
OneWire oneWire(PIN_SENSOR_DS18B20);
DallasTemperature DS18B20(&oneWire);


void updateSensorDS18B20(void) {
  DS18B20.requestTemperatures(); 
  valueTemperatureDS18B20 = DS18B20.getTempCByIndex(0);
}




EIoTCloudRestApi eiotcloud;
bool oldInputState;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);       // Initialize the BUILTIN_LED pin as an output
  eiotcloud.begin();

}

void loop() {



  if (checkTimedEvent(UPDATE_TIME_SENSORS, &lastMillisSensors)) {
    toggleLED();
    updateSensorDHT();
    updateSensorDS18B20();
    sendSensorData();
    updateServer();
  }
}

boolean checkTimedEvent (const unsigned long period, unsigned long * tempTimeValue) {
  if ((millis() - (*tempTimeValue)) < period) return false;
  *tempTimeValue = millis();
  return (true);
}


void toggleLED() {
  valueDiscrete = !valueDiscrete;
  digitalWrite(PIN_LED, valueDiscrete);
}

void updateServer()
{  
    eiotcloud.sendParameter(EIOT_CLOUD_ID_TEMPERATURE, valueTemperatureDHT);
    eiotcloud.sendParameter(EIOT_CLOUD_ID_HUMIDITY, valueHumidityDHT);
    eiotcloud.sendParameter(EIOT_CLOUD_ID_TEMPERATURE_DALLAS, valueTemperatureDS18B20);
}

void sendSensorData() 
{
    
               
    Serial.print("Humidity: ");
    Serial.print(valueHumidityDHT);
    Serial.println("%");
    Serial.print("Temperature DHT: ");
    Serial.print(valueTemperatureDHT);
    Serial.println("°C");
    Serial.print("Temperature DS18B20: ");
    Serial.print(valueTemperatureDS18B20);
    Serial.println("°C");
}


