// Import required libraries
#include "ThingSpeak.h"
#include "ESP8266WiFi.h"
int ledPin = 2; //On the ESP-12, the onboard LED is connected to pin 2

unsigned long myChannelNumber = 180204;
const char * myWriteAPIKey = "TCIDZX6BSW1KD29W";
 
// WiFi parameters
const char* ssid = "sde";
const char* pass = "anonbithdu";
float outputSignal = 0.0;

int status = WL_IDLE_STATUS;
WiFiClient  client;
 
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
 
pinMode(ledPin, OUTPUT);       // Initialize the BUILTIN_LED pin as an output
ThingSpeak.begin(client);
}
 
void loop() {
  Serial.println("LED low");
  digitalWrite(ledPin, LOW);   // Turn the LED on (Note that LOW is the voltage level
                               // but actually the LED is on; this is because 
                               // it is acive low on the ESP-01)
  
  delay(1000);                 // Wait for a second
  Serial.println("LED high");
  digitalWrite(ledPin, HIGH);  // Turn the LED off by making the voltage HIGH
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

  
}
