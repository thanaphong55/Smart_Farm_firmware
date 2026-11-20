#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* WIFI_SSID = "Toh_2.4G";
const char* WIFI_PASS = "Thanapong094";

#define SOIL_PIN 34
#define DHT_PIN  4
#define DHT_TYPE DHT22
#define FLOAT_PIN 27
#define RELAY_PIN 26

#define RELAY_ON LOW
#define RELAY_OFF HIGH

const int SOIL_DRY = 3200;
const int SOIL_WET = 1300;

DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
bool pumpOn = false;
int threshold = 35;
bool autoMode = true;

int readSoilPercent() {
  int raw = analogRead(SOIL_PIN);
  int percent = map(raw, SOIL_DRY, SOIL_WET, 0, 100);
  return constrain(percent, 0, 100);
}

void setPump(bool on) {
  pumpOn = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

void sendStatus() {
  StaticJsonDocument<256> doc;
  doc["soil"] = readSoilPercent();
  doc["temperature"] = isnan(dht.readTemperature()) ? 0 : dht.readTemperature();
  doc["humidity"] = isnan(dht.readHumidity()) ? 0 : dht.readHumidity();
  doc["water"] = (digitalRead(FLOAT_PIN) == LOW);
  doc["pump"] = pumpOn;
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nESP32 IP: " + WiFi.localIP().toString());

  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/pump", HTTP_POST, []() {
    StaticJsonDocument<100> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return;
    }
    setPump(doc["on"] | false);
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/threshold", HTTP_POST, []() {
    StaticJsonDocument<100> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server.send(400, "application/json", "{\"error\":\"invalid json\"}"); return;
    }
    threshold = constrain((int)(doc["value"] | 35), 10, 80);
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.begin();
}

void loop() {
  server.handleClient();
  if (autoMode) {
    bool waterAvailable = (digitalRead(FLOAT_PIN) == LOW);
    if (!waterAvailable) setPump(false); // ป้องกันปั๊มทำงานเมื่อน้ำหมด
    else if (readSoilPercent() < threshold) setPump(true);
    else setPump(false);
  }
  delay(100);
}
