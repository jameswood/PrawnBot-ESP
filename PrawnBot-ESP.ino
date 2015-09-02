// In your secrets.h file, define the following:
// const char *ssid = "";
// const char *password = "";
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
//void callback(char* topic, byte* incoming, unsigned int length);

WiFiClient wifiClient;
PubSubClient mqttclient(server, 1883, MQTTReceived, wifiClient);

void MQTTReceived(char* incomingTopic, byte* incomingPayload, unsigned int payloadLength){
  String incomingString = String((char *)incomingPayload);
  incomingString = incomingString.substring(0, payloadLength);
  Serial.println(String(incomingTopic) + ": " + incomingString);
  if(incomingString == "ping") mqttclient.publish(incomingTopic, (char*)("pong"));
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
  if(mqttclient.connect(myName, statusTopic, 2, true, (char*)("offline"))){
    mqttclient.publish(statusTopic, (char*)("online"));
    mqttclient.subscribe(controlTopic);
  }
}

void loop(){
  if(WiFi.status() != WL_CONNECTED){
    if(millis() - lastWifiConnectAttempt > networkTimeout){
      lastWifiConnectAttempt=millis();
      connectWifi();
    }
  } else if(!mqttclient.loop()){
    if(millis() - lastMQTTConnectAttempt > networkTimeout){
      lastMQTTConnectAttempt=millis();
      connectMQTT();
    }
  } else {
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
          mqttclient.publish(controlTopic, (byte*)("fed"), 3, true); //3 is length
        }
        else if(millis() - modeStartTime > jamTimeout){
          botMode = 2; //jammed
          Serial.println("Jammed");
          mqttclient.publish(statusTopic, (byte*)("jam"), 3, true); //3 is length
        }
      break;
    }
  }
}
