// In your secrets.h file, define the following:
// char ssid[] = "";
// char password[] = "";
// const char *mqttUsername = "";
// const char *mqttPassword = "";
// const int port = 1883;
// IPAddress server(192, 168, 1, 2);

#include <Bounce2.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"
const unsigned int motorPin = 14;
const unsigned int feedButtonPin = 0;
const unsigned int motorSwitchPin = 12;
Bounce feedButtonDebounced = Bounce();
Bounce motorSwitchDebounced = Bounce();
const unsigned int jamTimeout = 2000;
const unsigned int networkTimeout = 5000;
const unsigned int debounce = 50;
const char myName[] = "PrawnBot-ESP";
const char *controlTopic = "devices/prawnbot";
char statusTopic[] = "devices/prawnbot/status";
long modeStartTime = 0;
long lastWifiConnectAttempt = 0;
long lastMQTTConnectAttempt = 0;
int botMode = 0; // 0: Listening; 1: Dispensing; 2: Jam

WiFiClient wifiClient;
PubSubClient mqttClient(server, 1883, MQTTReceived, wifiClient);

void MQTTReceived(char* incomingTopic, byte* incomingPayload, unsigned int payloadLength){
  String incomingString = String((char *)incomingPayload);
  incomingString = incomingString.substring(0, payloadLength);
  Serial.println(String(incomingTopic) + ": " + incomingString);
  if(incomingString == "ping") mqttClient.publish(incomingTopic, (char*)("pong"));
  if(incomingString == "feed" && botMode != 2) botMode = 1;  //if not jammed
}

void setup(){
  pinMode(motorPin, OUTPUT);
  pinMode(feedButtonPin, INPUT_PULLUP);
  pinMode(motorSwitchPin, INPUT_PULLUP);
  motorSwitchDebounced.attach(motorSwitchPin);
  feedButtonDebounced.attach(feedButtonPin);
  motorSwitchDebounced.interval(debounce);
  feedButtonDebounced.interval(debounce);
  Serial.begin(115200);
  delay(10); //wait for serial
}

void connectWifi(){
  WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED){
    delay(250);
    Serial.print("-");
  }
  Serial.println("WiFi OK. IP: " + WiFi.localIP());
}

boolean connectMQTT(){
  if(mqttClient.connect(myName, mqttUsername, mqttPassword, statusTopic, 2, true, (char*)("offline"))){
    mqttClient.publish(statusTopic, (byte*)("online"), 6, true);
    mqttClient.subscribe(controlTopic);
  }
}

void loop(){
  if(WiFi.status() != WL_CONNECTED){
    if(millis() - lastWifiConnectAttempt > networkTimeout){
      lastWifiConnectAttempt=millis();
      connectWifi();
    }
  } else if(!mqttClient.loop()){
    if(millis() - lastMQTTConnectAttempt > networkTimeout){
      lastMQTTConnectAttempt=millis();
      connectMQTT();
    }
  }
  feedButtonDebounced.update();
  motorSwitchDebounced.update();
  switch (botMode){
    case 0: // Listening
      if(feedButtonDebounced.fell()){
        Serial.println("Button pressed");
        botMode = 1;
      }
    break;
    case 1: // Dispensing
      modeStartTime=millis();
      digitalWrite(motorPin, HIGH);
      if(motorSwitchDebounced.fell()){
        digitalWrite(motorPin, LOW);
        botMode = 0;
        modeStartTime = 0;
        Serial.println("Fed");
        mqttClient.publish(controlTopic, (char*)("fed"));
      }
      else if(millis() - modeStartTime > jamTimeout){
        botMode = 2; //jammed
        Serial.println("Jammed");
        mqttClient.publish(statusTopic, (char*)("jam"));
      }
    break;
  }
}
