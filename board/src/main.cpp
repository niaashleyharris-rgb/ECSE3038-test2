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

// Fan settings & state
bool hasSettings = false;
String currentMode = "off";
float maxHumidity = 100.0;
float lastHumidity = 55.0;
bool fanIsOn = false;

unsigned long lastReadingMillis = 0;
const unsigned long READING_INTERVAL = 10000; // 10 seconds

void updateFanState() {
  bool desiredState = false;

  if (!hasSettings) {
    desiredState = false;
  } else if (currentMode == "on") {
    desiredState = true;
  } else if (currentMode == "off") {
    desiredState = false;
  } else if (currentMode == "auto") {
    desiredState = (lastHumidity > maxHumidity);
  }

  digitalWrite(FAN_PIN, desiredState ? HIGH : LOW);

  // B5: Publish on/off retained ONLY when state changes
  if (desiredState != fanIsOn) {
    fanIsOn = desiredState;
    if (mqttClient.connected()) {
      mqttClient.publish(topicState.c_str(), fanIsOn ? "on" : "off", true);
    }
    Serial.print("Fan state changed to: ");
    Serial.println(fanIsOn ? "on" : "off");
  }
}

// B3: Handle incoming settings JSON
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != topicSettings) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.println("Ignored invalid JSON on fan/settings");
    return;
  }

  // Validate required fields exist
  if (!doc["mode"].is<const char*>() || !doc["max_humidity"].is<float>()) {
    // Also allow int for max_humidity in JSON
    if (!doc["mode"].is<const char*>() || !doc["max_humidity"].is<int>()) {
      Serial.println("Ignored settings with missing/invalid fields");
      return;
    }
  }

  String newMode = doc["mode"].as<String>();
  float newMax = doc["max_humidity"].as<float>();

  if (newMode != "auto" && newMode != "on" && newMode != "off") {
    Serial.println("Ignored invalid mode value");
    return;
  }

  currentMode = newMode;
  maxHumidity = newMax;
  hasSettings = true;

  Serial.printf("Applied settings -> mode: %s, max_humidity: %.1f\n", currentMode.c_str(), maxHumidity);

  // B4: React immediately when new settings arrive
  updateFanState();
}

