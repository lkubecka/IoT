// Import required libraries
#include "ThingSpeak.h"
#include "ESP8266WiFi.h"
const int PIN_LED = 2; //On the ESP-12, the onboard LED is connected to pin 2
const int PIN_SENSOR_DHT = 5;     //GPIO5 = AM2321 signal

unsigned long myChannelNumber = 180204;
const char * myWriteAPIKey = "TCIDZX6BSW1KD29W";
 
// WiFi parameters
const char* ssid = "sde";
const char* pass = "anonbithdu";
float outputSignal = 0.0;

String webString="";     // String to display
unsigned long lastMillisSensors = 0;        // will store last temp was read, use "unsigned long" for variables that hold time

const unsigned long UPDATE_TIME_SENSORS = 2500;//ms
const unsigned long UPDATE_TIME_MG811 = 200;//ms
const unsigned long UPDATE_TIME_STATUS_LEDS = 250;//ms
const unsigned long UPDATE_TIME_STATUS_VPINS = 500;//ms
const unsigned long UPDATE_TIME_VALUE_VPINS = 500;//ms

boolean isFaultDHT = false;

float valueTemperatureDHT = 0.0;
float valueHumidityDHT = 0.0;

int status = WL_IDLE_STATUS;
WiFiClient  client;


#include <DHT.h>

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

 
void setup(void)
{ 
// Start Serial
Serial.begin(9600);


 
// Connect to WiFi
WiFi.begin(ssid, pass);
while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
}
Serial.println("");
Serial.println("WiFi connected");
 
// Print the IP address
Serial.println(WiFi.localIP());
 
pinMode(PIN_LED, OUTPUT);       // Initialize the BUILTIN_LED pin as an output
ThingSpeak.begin(client);
}
 
void loop() {
  Serial.println("LED low");
  digitalWrite(PIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
                               // but actually the LED is on; this is because 
                               // it is acive low on the ESP-01)
  
  delay(1000);                 // Wait for a second
  Serial.println("LED high");
  digitalWrite(PIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  delay(2000);                 // Wait for two seconds (to demonstrate the active low LED)
  ThingSpeak.writeField(myChannelNumber, 1, outputSignal, myWriteAPIKey);
  
  char dato = Serial.read();
  if (dato == 'S'){
    Serial.write(45);
    Serial.println("S entered.");
    outputSignal = 1.0;    
  }
  if (dato == 'E'){
    Serial.write(45);
    Serial.println("E entered.");
    outputSignal = 0.0;    
  }

  if (checkTimedEvent(UPDATE_TIME_SENSORS, &lastMillisSensors)) {
    updateSensorDHT();
    //updateSensorOneWire();
    broadcastSensorData();
  }
  
}

boolean checkTimedEvent (const unsigned long period, unsigned long * tempTimeValue) {
  if ((millis() - (*tempTimeValue)) < period) return false;
  *tempTimeValue = millis();
  return (true);
}

void broadcastSensorData() 
{

    ThingSpeak.writeField(myChannelNumber, 2, valueTemperatureDHT, myWriteAPIKey);
    ThingSpeak.writeField(myChannelNumber, 3, valueHumidityDHT, myWriteAPIKey);
    Serial.print("Humidity: ");
    Serial.print(valueHumidityDHT);
    Serial.println("%");
    Serial.print("Temperature: ");
    Serial.print(valueTemperatureDHT);
    Serial.println("°C");
}
