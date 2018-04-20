/*
 * Connect to AP "arduino_car", password = "password"
 * Open browser, visit 192.168.4.1
 */
#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <Servo.h>

const char *ssid = "arduino_car";
const char *password = "password";
int stateLED = LOW;

//define variables
#define SERV1 D5
#define M1F D6
#define M1R D2
#define M2F D7
#define M2R D1

Servo s1; //servo 1

int pos1min = 75; //servo 1 minimum position
int pos1mid = 90;
int pos1max = 105; //servo 1 maximum position


ESP8266WebServer server(80);

void cForward(){
  analogWrite(M1R, 1024);
  analogWrite(M2R, 1024);
  analogWrite(M1F, 100);
  analogWrite(M2F, 100);
}

void cStop(){
  analogWrite(M1F, 1024);
  analogWrite(M2F, 1024);
  analogWrite(M1R, 1024);
  analogWrite(M2R, 1024);
}

void cBackward(){
  analogWrite(M1F, 1024);
  analogWrite(M2F, 1024);
  analogWrite(M1R, 100);
  analogWrite(M2R, 100);  
}

void handleRoot() {
    response();
}

void handleLeft() {
  s1.attach(SERV1);
  s1.write(pos1min);
  response();
}

void handleUp() {
  s1.attach(SERV1);
  s1.write(pos1mid);
  response();
}

void handleRight() {
  s1.attach(SERV1);
  s1.write(pos1max);
  response();
}

void handleForward() { 
  stateLED = LOW;
  digitalWrite(LED_BUILTIN, stateLED);
  cForward();
  response();
}

void handleBackward() {
 
  stateLED = HIGH;
  digitalWrite(LED_BUILTIN, stateLED);
  cStop();
  response();
}

void handleMove(){
  if(server.hasArg("speed") == false || server.hasArg("angle") == false){
    response();
    return;  
  }

  int cSpeed = server.arg("speed").toInt();
  int cAngle = server.arg("angle").toInt();
  
  if(cSpeed > 0){
    cForward();
  } else if (cSpeed == 0) {
    cStop();  
  } else {
    cBackward();  
  }

  s1.attach(SERV1);
  s1.write(cAngle);
  response();
}

void response(){
  const String a = "xx";
  server.send(200, "text/html", "speed = " + server.arg("speed") + " / angle = " + server.arg("angle"));
}

void setup() {

  //attach and set servos' angles
  s1.attach(SERV1);
  s1.write(pos1mid);
  
  pinMode(M1F, OUTPUT);
  pinMode(M1R, OUTPUT);
  pinMode(M2F, OUTPUT);
  pinMode(M2R, OUTPUT);

  analogWrite(M1F, 1024);
  analogWrite(M2F, 1024);
  analogWrite(M1R, 1024);
  analogWrite(M2R, 1024);
  
  delay(1000);
  Serial.begin(9600);
  Serial.println();

  WiFi.softAP(ssid, password);

  IPAddress apip = WiFi.softAPIP();

  server.on("/", handleRoot);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/up", handleUp);
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/move", handleMove);
  
  server.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, stateLED);
}

void loop() {
    server.handleClient();
}
