
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include <PubSubClient.h>
#include <math.h>

// WiFi configuration
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// Azure API address
const char* API_URL =
  "http://iot-jhardware.chilecentral.cloudapp.azure.com/readings";

// DHT22 sensor
#define DHT_PIN 15
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

// Fan LED
#define FAN_LED_PIN 2

// Timing
unsigned long lastReading = 0;
const unsigned long INTERVAL = 10000;

// Device MAC address
String deviceMAC;

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
}

// MQTT configuration
const char* MQTT_SERVER = "135.232.203.144";
const int MQTT_PORT = 1883;

// Using MQTT name from Tutorial 9
const char* MQTT_NAME = "tobias7140";

WiFiClient mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);

String statusTopic;

// MQTT settings topic
String settingsTopic;

// Current fan settings
String fanMode = "off";
float maxHumidity = 100.0;

// Tracks whether valid settings have been received
bool settingsReceived = false;

// Current humidity and fan state
float currentHumidity = NAN;
bool fanState = false;


// B4 - Control the fan LED
void updateFan() {

  bool newFanState = false;

  // Keep fan off until settings arrive
  if (settingsReceived) {

    if (fanMode == "on") {
      newFanState = true;
    }

    else if (fanMode == "off") {
      newFanState = false;
    }

    else if (fanMode == "auto") {
      if (!isnan(currentHumidity)) {
        newFanState = currentHumidity > maxHumidity;
      }
    }
  }

  // Update fan only when its state changes
  if (newFanState != fanState) {

    fanState = newFanState;

    digitalWrite(
      FAN_LED_PIN,
      fanState ? HIGH : LOW
    );

    Serial.print("Fan state: ");
    Serial.println(fanState ? "ON" : "OFF");
  }
}


// B3 - Receive and validate MQTT settings
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Received MQTT topic: ");
  Serial.println(topic);

  // Only process fan settings messages
  if (String(topic) != settingsTopic) {
    Serial.println("Ignored: Different MQTT topic");
    return;
  }

  Serial.println("MQTT settings received!");

  // Parse incoming JSON directly from payload
  JsonDocument doc;

  DeserializationError error = deserializeJson(
    doc,
    payload,
    length
  );

  // Reject malformed JSON
  if (error) {
    Serial.println("Rejected: Invalid JSON");
    return;
  }

  // Reject missing fields
  if (doc["mode"].isNull() ||
      doc["max_humidity"].isNull()) {

    Serial.println("Rejected: Missing fields");
    return;
  }

  // Verify field types
  if (!doc["mode"].is<const char*>() ||
      (!doc["max_humidity"].is<JsonFloat>() &&
       !doc["max_humidity"].is<JsonInteger>())) {

    Serial.println("Rejected: Invalid field types");
    return;
  }

  // Extract proposed settings
  String newMode = doc["mode"].as<String>();
  float newMaxHumidity =
      doc["max_humidity"].as<float>();

  // Reject invalid modes
  if (newMode != "auto" &&
      newMode != "on" &&
      newMode != "off") {

    Serial.println("Rejected: Invalid mode");
    return;
  }

  // Reject invalid humidity thresholds
  if (!isfinite(newMaxHumidity) ||
      newMaxHumidity < 0 ||
      newMaxHumidity > 100) {

    Serial.println("Rejected: Invalid humidity limit");
    return;
  }

  // All checks passed: update settings together
  fanMode = newMode;
  maxHumidity = newMaxHumidity;
  settingsReceived = true;

  Serial.println("Settings accepted!");
  Serial.print("Mode: ");
  Serial.println(fanMode);

  Serial.print("Max humidity: ");
  Serial.println(maxHumidity);

  // React immediately to new settings
  updateFan();
}


void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT... ");

    // Use the ESP32 MAC to make a unique client ID
    String clientId = "ESP32-" + deviceMAC;
    clientId.replace(":", "");

    bool connected = mqttClient.connect(
      clientId.c_str(),
      nullptr,
      nullptr,
      statusTopic.c_str(),
      1,
      true,
      "offline"
    );

    if (connected) {
      Serial.println("Connected!");

      // Publish online status
      mqttClient.publish(
        statusTopic.c_str(),
        "online",
        true
      );

      // Subscribe to fan settings
      if (mqttClient.subscribe(settingsTopic.c_str(), 1)) {
        Serial.println("Subscribed to fan settings!");
        Serial.print("Subscribed topic: ");
        Serial.println(settingsTopic);
      } else {
        Serial.println("Failed to subscribe to settings");
      }

    } else {
      Serial.print("Failed, MQTT state: ");
      Serial.println(mqttClient.state());

      delay(2000);
    }
  }
}


void sendReading(float humidity) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected");
    return;
  }

  HTTPClient http;

  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", DEVICE_KEY);

  JsonDocument doc;
  doc["mac"] = deviceMAC;
  doc["humidity"] = humidity;

  String payload;
  serializeJson(doc, payload);

  int responseCode = http.POST(payload);

  Serial.print("HTTP response: ");
  Serial.println(responseCode);

  if (responseCode > 0) {
    Serial.println(http.getString());
  }

  http.end();
}


void setup() {
  Serial.begin(115200);

  // Configure fan LED
  pinMode(FAN_LED_PIN, OUTPUT);
  digitalWrite(FAN_LED_PIN, LOW);

  dht.begin();
  connectWiFi();

  deviceMAC = WiFi.macAddress();

  Serial.print("ESP32 MAC Address: ");
  Serial.println(deviceMAC);

  // Build the MQTT status topic
  statusTopic = String("ecse3038/") +
                MQTT_NAME + "/status";

  // Construct fan settings topic
  settingsTopic = String("ecse3038/") +
                MQTT_NAME + "/fan/settings";

  // Configure MQTT broker
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setKeepAlive(10);
  mqttClient.setBufferSize(512);
  mqttClient.setCallback(mqttCallback);

  // Connect and publish online status
  connectMQTT();

  lastReading = millis();
}

void loop() {

  // Reconnect WiFi if necessary
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Reconnect MQTT if disconnected
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  // Keep MQTT connection active
  mqttClient.loop();

  // Continue B1 humidity reporting
  if (millis() - lastReading >= INTERVAL) {
    lastReading = millis();

    float humidity = dht.readHumidity();

    if (isnan(humidity)) {
      Serial.println("DHT22 reading failed!");
      return;
    }

    Serial.print("Humidity: ");
    Serial.println(humidity);

    // Store latest humidity for fan control
    currentHumidity = humidity;

    // Update fan based on humidity
    updateFan();

    sendReading(humidity);
  }
}
