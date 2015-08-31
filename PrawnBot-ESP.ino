#include <Bounce2.h>
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
boolean somethingToSay = false;
String messageToReport;
long modeStartTime = 0;
unsigned int lastAnnounced = 5;
int botMode = 0; // 0: Listening; 1: Dispensing; 2: Jam
void callback(char* topic, byte* incoming, unsigned int length);
Bounce motorSwitchDebounced = Bounce();
Bounce feedButtonDebounced = Bounce();


WiFiClient wifiClient;
PubSubClient mqttclient(server, 1883, callback, wifiClient);

void callback(char* theTopic, byte* incoming, unsigned int length) {
  String incomingMessage = String((char *)incoming);
  incomingMessage = incomingMessage.substring(0, length);
  Serial.print(String(theTopic));
  Serial.print(": ");
  Serial.println(incomingMessage);
  if (incomingMessage == "feed") botMode = 1;
  else if (incomingMessage == "ping") mqttclient.publish(topic, (char*)("pong"));
}

void setup() {
  pinMode(motorPin, OUTPUT);
  pinMode(feedButtonPin, INPUT_PULLUP);
  pinMode(motorSwitchPin, INPUT_PULLUP);
  motorSwitchDebounced.attach(motorSwitchPin);
  feedButtonDebounced.attach(feedButtonPin);
  motorSwitchDebounced.interval(50);
  feedButtonDebounced.interval(50);
  Serial.begin(115200);
  delay(10);
  WiFi.begin(ssid, password);
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
  if (mqttclient.connect(clientName)) {
    mqttclient.publish(topic, (char*)("WiFi GO"));
    mqttclient.subscribe(topic);
  }
}

void loop() {
  if(WiFi.status() != WL_CONNECTED) getOnline();
  feedButtonDebounced.update();
  motorSwitchDebounced.update();
  mqttclient.loop(); //
  if (motorJammed == true) botMode = 3;
  switch (botMode) {
    case 0: // Listening
      if(lastAnnounced != 0) {
        Serial.println("Listening");
        lastAnnounced = 0;
      }
      if (feedButtonDebounced.fell()){
        Serial.println("Button pressed");
        botMode = 1;
        modeStartTime = 0;
      }
    break;
    
    case 1: // Dispensing
      modeStartTime=millis();
      if(lastAnnounced != 1) {
        Serial.println("Dispensing");
        lastAnnounced = 1;
      }
      digitalWrite(motorPin, HIGH);
      if (feedButtonDebounced.fell()){
        digitalWrite(motorPin, LOW);
        botMode = 0;
        modeStartTime = 0;
        somethingToSay = true;
        messageToReport="fed";
      }
      else if (millis() - modeStartTime > dispenseTimeout) {
        botMode = 3;
        modeStartTime = 0;
      }
    break;

    case 2: // Jammed
      if(lastAnnounced != 2) {
        Serial.println("Jammed");
        lastAnnounced = 2;
      }
      if (messageToReport != "jam") somethingToSay = true;
      motorJammed = true;
      messageToReport="jam";
    break;
  }
  
  if (somethingToSay) {
    Serial.println("Send MQTT");
    int messageLength = messageToReport.length() + 1;
    char messageChar[messageLength];
    messageToReport.toCharArray(messageChar, messageLength);
    mqttclient.publish(topic, messageChar);
    somethingToSay = false;
  }
}
