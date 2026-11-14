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

// B2: Connect/Reconnect MQTT with Last Will
void reconnectMqtt() {
  while (!mqttClient.connected()) {
    String clientId = "esp32-fan-" + WiFi.macAddress();
    Serial.print("Connecting to MQTT broker...");

    // Last will: topicStatus, QoS 0, retained = true, payload = "offline"
    if (mqttClient.connect(clientId.c_str(), nullptr, nullptr, topicStatus.c_str(), 0, true, "offline")) {
      Serial.println("connected!");
      // Publish online, retained
      mqttClient.publish(topicStatus.c_str(), "online", true);
      // Publish initial state retained so topic reflects current state
      mqttClient.publish(topicState.c_str(), fanIsOn ? "on" : "off", true);
      // Subscribe to settings
      mqttClient.subscribe(topicSettings.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 2s...");
      delay(2000);
    }
  }
}

// B1: Post humidity reading to API
void sendReading(float humidity) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(API_BASE_URL) + "/readings";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", DEVICE_KEY);

  JsonDocument doc;
  doc["mac"] = WiFi.macAddress();
  doc["humidity"] = humidity;

  String body;
  serializeJson(doc, body);

  int httpCode = http.POST(body);
  Serial.printf("POST /readings (humidity=%.1f) -> HTTP %d\n", humidity, httpCode);
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);
  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("ESP32 MAC Address: ");
  Serial.println(WiFi.macAddress());

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMqtt();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastReadingMillis >= READING_INTERVAL || lastReadingMillis == 0) {
    lastReadingMillis = now;
    float h = dht.readHumidity();
    if (!isnan(h)) {
      lastHumidity = h;
      updateFanState();
      sendReading(h);
    } else {
      Serial.println("Failed to read from DHT22!");
    }
  }
}