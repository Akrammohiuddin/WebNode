#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// WiFi Credentials
const char* ssid = "Airtel_KITKAT";
const char* password = "SFMohiuddin";

// Adafruit IO Credentials
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "kiterobotics"
#define AIO_KEY         "aio_vKuJ72psl9lErkbxtUbPSEjiR2mg"

// Pin definitions
#define DHTPIN D4
#define DHTTYPE DHT11
#define ledPin D1

// Global objects
DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe ledFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/LED");
Adafruit_MQTT_Publish sensorFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/LED");

bool ledState = false;

// HTML Web Page
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>LED Control</title>
  <style>
    body { background-color: black; color: white; font-family: Arial; text-align: center; }
    .title { color: #32CD32; font-size: 36px; margin: 20px auto 10px; font-weight: bold; }
    .container { background: white; color: black; border-radius: 12px; padding: 30px; max-width: 350px; margin: 40px auto; box-shadow: 0 0 20px rgba(0,0,0,0.5); }
    .heading { font-size: 28px; font-weight: bold; margin-bottom: 20px; }
    .switch { position: relative; display: inline-block; width: 60px; height: 34px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
    .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: #4CAF50; }
    input:checked + .slider:before { transform: translateX(26px); }
    .status { margin-top: 15px; font-weight: bold; color: red; }
    .readings { margin-top: 20px; font-size: 16px; }
  </style>
</head>
<body>
  <div class="title">AKRAM KITE ROBOTICS</div>
  <div class="container">
    <div class="heading">LED Control</div>
    <label class="switch">
      <input type="checkbox" id="ledSwitch" onchange="toggleLED(this)">
      <span class="slider"></span>
    </label>
    <div class="status" id="ledStatus">LED is OFF</div>
    <div class="readings" id="sensorData">Temp: -- °C | Humidity: --%</div>
  </div>
<script>
  function toggleLED(el) {
    const state = el.checked ? "on" : "off";
    fetch(`/led?state=${state}`).then(() => {
      document.getElementById("ledStatus").textContent = state === "on" ? "LED is ON" : "LED is OFF";
      document.getElementById("ledStatus").style.color = state === "on" ? "green" : "red";
    });
  }
  function getSensorData() {
    fetch("/sensor").then(res => res.json()).then(data => {
      document.getElementById("sensorData").innerHTML = 
        `Temp: ${data.temperature} °C | Humidity: ${data.humidity}%`;
    });
  }
  setInterval(getSensorData, 2000);
</script>
</body>
</html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleLED() {
  String state = server.arg("state");
  ledState = (state == "on");
  digitalWrite(ledPin, ledState ? HIGH : LOW);
  server.send(200, "text/plain", "OK");

  // Publish to Adafruit IO
  if (ledState) {
    sensorFeed.publish("ON");
  } else {
    sensorFeed.publish("OFF");
  }
}

void handleSensor() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  String json = "{\"temperature\":" + String(t) + ", \"humidity\":" + String(h) + "}";
  server.send(200, "application/json", json);

  // Publish to Adafruit IO
  if (!isnan(t) && !isnan(h)) {
    String payload = "Temp: " + String(t) + "°C, Hum: " + String(h) + "%";
    sensorFeed.publish(payload.c_str());
  }
}

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

void MQTT_connect() {
  if (mqtt.connected()) return;
  int8_t ret;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println("MQTT connect failed, retrying...");
    mqtt.disconnect();
    delay(5000);
  }
  Serial.println("MQTT Connected");
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  dht.begin();

  connectWiFi();
  mqtt.subscribe(&ledFeed);

  server.on("/", handleRoot);
  server.on("/led", handleLED);
  server.on("/sensor", handleSensor);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
  MQTT_connect();
  mqtt.processPackets(10);

  Adafruit_MQTT_Subscribe *sub;
  while ((sub = mqtt.readSubscription(10))) {
    if (sub == &ledFeed) {
      String cmd = (char *)ledFeed.lastread;
      ledState = (cmd == "ON");
      digitalWrite(ledPin, ledState ? HIGH : LOW);
      Serial.println("LED updated from Adafruit IO: " + cmd);
    }
  }
}
