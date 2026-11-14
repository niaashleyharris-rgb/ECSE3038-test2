#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "secrets.h"

#define DHT_PIN 15
#define DHT_TYPE DHT22
#define FAN_PIN 2

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_BROKER = "135.232.203.144";
const int MQTT_PORT = 1883;

String topicSettings = String("ecse3038/") + MQTT_NAME + "/fan/settings";
String topicState    = String("ecse3038/") + MQTT_NAME + "/fan/state";
String topicStatus   = String("ecse3038/") + MQTT_NAME + "/status";

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

