#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "secrets.h"
const int feedButtonPin = 0;
const int motorPin = 14;
const int switchPin = 2;
const int dispenseTimeout = 2000;
const int feedButtonHoldTime = 50;
const int debounce = 50;
boolean disableMotor = false;
boolean motorJammed = false;
boolean wifiConnected = false;
const String clientName = "PrawnBot-ESP";

WiFiClient wifiClient;
PubSubClient mqtt(server, port, callback, wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

void setup() {
  pinMode(feedButtonPin, INPUT_PULLUP);
  Serial.begin(115200);
  delay(10);
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Connecting to ");
  Serial.print(server);
  Serial.print(" as ");
  Serial.println(clientName);
  
  if (mqtt.connect((char*) clientName.c_str())) {
    Serial.println("Connected to MQTT broker");
    Serial.print("Topic is: ");
    Serial.println(topic);
    
    if (mqtt.publish(topic, "online")) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  } else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }
}

void loop() {
  checkButton();
}


void checkButton() {
  if (digitalRead(feedButtonPin) == LOW) {
    long feedButtonPressedTime = millis();
    while (digitalRead(feedButtonPin) == LOW) {
      if (millis() - feedButtonPressedTime > feedButtonHoldTime) {
        Serial.println("Button pressed");
        dispense();
        break;
      };
    };
  };
}
