// Import required libraries
#include "Arduino.h" 
#include "ThingSpeak.h"
#include "ESP8266WiFi.h"
#include <DHT.h>
#include "DallasTemperature.h"
#include <rBase64.h>

// EasyIoT server definitions
// http://iot-playground.com/blog/2-uncategorised/86-easyiot-cloud-mqtt-api-v1
#define EIOT_USERNAME    "lkubecka"
#define EIOT_PASSWORD    "L1borovo"
#define EIOT_IP_ADDRESS  "176.9.90.169" // cloud.iot-playground.com
#define EIOT_PORT        1883
#define EIOT_NODE        "ESP"

#define EIOT_CLOUD_ADDRESS     "cloud.iot-playground.com"
#define EIOT_CLOUD_PORT        40404

// EasyIoT Cloud definitions - change EIOT_CLOUD_INSTANCE_PARAM_ID
#define EIOT_CLOUD_INSTANCE_PARAM_ID    "58448d11c943a07ae877855b/xBjMWbeSsRHRkO72"

const int PIN_LED = 2; //On the ESP-12, the onboard LED is connected to pin 2
const int PIN_SENSOR_DHT = 5;     //GPIO5 = AM2321 signal
const int PIN_SENSOR_DS18B20 = 4;     //GPIO4 = DS18B20 one-wire signal

unsigned long myChannelNumber = 180204;
const char * myWriteAPIKey = "TCIDZX6BSW1KD29W";
 
// WiFi parameters
const char* AP_SSID = "sde";
const char* AP_PASSWORD = "anonbithdu";


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



// Initialize DHT sensor 
// NOTE: For working with a faster than ATmega328p 16 MHz Arduino chip, like an ESP8266,
// you need to increase the threshold for cycle counts considered a 1 or 0.
// You can do this by passing a 3rd parameter for this threshold.  It's a bit
// of fiddling to find the right value, but in general the faster the CPU the
// higher the value.  The default for a 16mhz AVR is a value of 6.  For an
// Arduino Due that runs at 84mhz a value of 30 works.
// This is for the ESP8266 processor on ESP-01 
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

/*
 * Easy IoT server
 */

#define USER_PWD_LEN 40
char unameenc[USER_PWD_LEN];

  

  
void EasyIoTsetup() {
  char uname[USER_PWD_LEN];
  String str = String(EIOT_USERNAME)+":"+String(EIOT_PASSWORD);  
  str.toCharArray(uname, USER_PWD_LEN); 
  memset(unameenc,0,sizeof(unameenc));
  rbase64_encode(unameenc, uname, strlen(uname));
}

void setup(void)
{ 
  // Start Serial
  Serial.begin(9600);
    
  pinMode(PIN_LED, OUTPUT);       // Initialize the BUILTIN_LED pin as an output
  
  wifiConnect();
  //ThingSpeak.begin(client);
  //EasyIoTsetup();
}
 
void loop() {
  
  readSerialInput();

  

  if (checkTimedEvent(UPDATE_TIME_LED, &lastMillisSensors)) {
     
  }

  if (checkTimedEvent(UPDATE_TIME_SENSORS, &lastMillisSensors)) {
    toggleLED();
    updateSensorDHT();
    updateSensorDS18B20();
    //updateSensorOneWire();
    sendSensorData();
    sendTeperature(valueTemperatureDHT);
  }

}

void readSerialInput() {
  char inputChar = Serial.read();
  if (inputChar == 'H'){
    Serial.write(45);
    Serial.println("H entered.");
    valueDiscrete = 1.0;    
  }
  if (inputChar == 'L'){
    Serial.write(45);
    Serial.println("L entered.");
    valueDiscrete = 0.0;    
  }
}

void toggleLED() {
  valueDiscrete = !valueDiscrete;
  digitalWrite(PIN_LED, valueDiscrete);
}

boolean checkTimedEvent (const unsigned long period, unsigned long * tempTimeValue) {
  if ((millis() - (*tempTimeValue)) < period) return false;
  *tempTimeValue = millis();
  return (true);
}

void wifiConnect()
{
  Serial.print("Connecting to AP");
  WiFi.begin(AP_SSID, AP_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println(WiFi.localIP());
}

void sendTeperature(float temp)
{  
//   WiFiClient client;
//   
//   while(!client.connect(EIOT_CLOUD_ADDRESS, EIOT_CLOUD_PORT)) {
//    Serial.println("connection failed");
//    wifiConnect(); 
//  }

  String url = "";
  // URL: /RestApi/SetParameter/[instance id]/[parameter id]/[value]
  url += "/RestApi/SetParameter/"+ String(EIOT_CLOUD_INSTANCE_PARAM_ID) + "/"+String(temp); // generate EasIoT cloud update parameter URL

  Serial.print("POST data to URL: ");
  Serial.println(url);
  
  WiFi.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + String(EIOT_CLOUD_ADDRESS) + "\r\n" + 
               "Connection: close\r\n" + 
               "Content-Length: 0\r\n" + 
               "\r\n");

//  delay(100);
//  while(client.available()) {
//    String line = client.readStringUntil('\r');
//    Serial.print(line);
//  }
  
  Serial.println();
  Serial.println("Connection closed");
}

void sendSensorData() 
{
    
//    ThingSpeak.writeField(myChannelNumber, 1, valueDiscrete, myWriteAPIKey);
//    ThingSpeak.writeField(myChannelNumber, 2, valueTemperatureDHT, myWriteAPIKey);
//    ThingSpeak.writeField(myChannelNumber, 3, valueHumidityDHT, myWriteAPIKey);
//    ThingSpeak.writeField(myChannelNumber, 4, valueTemperatureDS18B20, myWriteAPIKey);        
               
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
