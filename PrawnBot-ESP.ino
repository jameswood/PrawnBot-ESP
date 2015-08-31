#include <SPI.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"
const int feedButtonPin = 0;
const int motorPin = 14;
const int motorSwitchPin = 12;
const int dispenseTimeout = 2000;
const int debounce = 100;
boolean motorJammed = false;
char clientName[] = "PrawnBot-ESP";
boolean somethingToSay = true;
String messageToReport = "Online";
long modeStartTime = 0;
int lastAnnounced;
int botMode = 2; // 0: Listening; 1: Dispensing; 2: Resetting; 3: Jam

void callback(char* topic, byte* incoming, unsigned int length);

WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

void callback(char* theTopic, byte* incoming, unsigned int length) {
  String incomingMessage = String((char *)incoming);
  incomingMessage = incomingMessage.substring(0, length);
  Serial.print(String(theTopic));
  Serial.print(": ");
  Serial.println(incomingMessage);
  if (incomingMessage == "feed") botMode = 1;
  else if (incomingMessage == "ping") client.publish(topic, (char*)("pong"));
}

void setup() {
  pinMode(feedButtonPin, INPUT_PULLUP);
  pinMode(motorPin, OUTPUT);
  pinMode(motorSwitchPin, INPUT_PULLUP);
  Serial.begin(115200);
  delay(10);
  WiFi.begin(ssid, password);
  getOnline();
}

void getOnline(){
   while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(":) ");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  if (client.connect(clientName)) {
    client.publish(topic, (char*)("online"));
    client.subscribe(topic);
  }
}

void loop() {
  if(WiFi.status() != WL_CONNECTED) getOnline();
  client.loop();
  if (motorJammed == true) botMode = 3;
  switch (botMode) {
    case 0: // Listening
      if(lastAnnounced != 0) {
        Serial.println("Listening");
        lastAnnounced = 0;
      }
      if (digitalRead(feedButtonPin) == HIGH) modeStartTime = 0;
      else if (digitalRead(feedButtonPin) == LOW && modeStartTime == 0) modeStartTime = millis();
      else if (digitalRead(feedButtonPin) == LOW && (millis() - modeStartTime > debounce)) {
        Serial.println("Button pressed");
        botMode = 1;
        modeStartTime = 0;
      }
    break;
    
    case 1: // Dispensing
      if(lastAnnounced != 1) {
        Serial.println("Dispensing");
        lastAnnounced = 1;
      }
      if (modeStartTime == 0) modeStartTime = millis();
      if ((digitalRead(motorSwitchPin) == LOW) && (millis() - modeStartTime > debounce)) {
        digitalWrite(motorPin, HIGH);
      } else if ((digitalRead(motorSwitchPin) == HIGH) && (millis() - modeStartTime > debounce)) {
        digitalWrite(motorPin, LOW);
        botMode = 2;
        modeStartTime = 0;
        if (messageToReport != "fed") somethingToSay = true;
        messageToReport="fed";
      }
      if ((modeStartTime > 0) && (millis() - modeStartTime > dispenseTimeout)) {
        botMode = 3;
        modeStartTime = 0;
      }
    break;
    
    case 2: // Resetting
      if(lastAnnounced != 2) {
        Serial.println("Resetting Motor");
        lastAnnounced = 2;
      }
      if (modeStartTime == 0) modeStartTime = millis();
      if ((digitalRead(motorSwitchPin) == HIGH)) { // && (millis() - modeStartTime > debounce)) {
        digitalWrite(motorPin, HIGH);
      }
      else if ((digitalRead(motorSwitchPin) == LOW) && (millis() - modeStartTime > debounce)) {
        digitalWrite(motorPin, LOW);
        botMode = 0;
        modeStartTime = 0;
        if (messageToReport != "ready") somethingToSay = true;
        messageToReport="ready";
      }
      if ((modeStartTime > 0) && (millis() - modeStartTime > dispenseTimeout)) {
        botMode = 3;
        modeStartTime = 0;
      }
    break;

    case 3: // Jammed
      if(lastAnnounced != 3) {
        Serial.println("Resetting Motor");
        lastAnnounced = 3;
      }
      if (messageToReport != "jam") somethingToSay = true;
      motorJammed = true;
      messageToReport="jam";
  }
  
  if (somethingToSay) {
    Serial.println("Send MQTT");
    int messageLength = messageToReport.length() + 1;
    char messageChar[messageLength];
    messageToReport.toCharArray(messageChar, messageLength);
    client.publish(topic, messageChar);
    somethingToSay = false;
  }
}
