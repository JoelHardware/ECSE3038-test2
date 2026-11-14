
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include <PubSubClient.h>

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
const char* MQTT_NAME = "tobia7140";

WiFiClient mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);

String statusTopic;


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

      // Publish retained online status
      mqttClient.publish(
        statusTopic.c_str(),
        "online",
        true
      );

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

  dht.begin();
  connectWiFi();

  deviceMAC = WiFi.macAddress();

  Serial.print("ESP32 MAC Address: ");
  Serial.println(deviceMAC);

  // Build the MQTT status topic
  statusTopic = String("ecse3038/") +
                MQTT_NAME + "/status";

  // Configure MQTT broker
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setKeepAlive(10);

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

    sendReading(humidity);
  }
}
