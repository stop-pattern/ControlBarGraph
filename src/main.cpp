#include "FS.h"
#include "SPIFFS.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <driver/ledc.h>
#include <map>

#define FORMAT_SPIFFS_IF_FAILED true

constexpr uint8_t pin_ain = 39;  // 可変抵抗
constexpr uint8_t pin_ledR = 26; // LED緑
constexpr uint8_t pin_ledG = 25; // LED赤
constexpr uint8_t pin_btn = 13;  // タクトスイッチ
constexpr uint8_t pin_out = 4;   // PFM出力
// constexpr uint8_t pin_out = 2;      // 試作時出力

namespace{
// const char *ssid = "BarGraqh";
// const char *password = "password";
const char *ssid = "BarGraqh";
const char *password = "password";
const IPAddress ip(192, 168, 0, 1);
const IPAddress subnet(255, 255, 255, 0);
const char *hostname = "esp32";

// GPIOピン
std::map<String, uint8_t> switchMap = {{"red", pin_ledR}, {"green", pin_ledG}};

// 状態管理
std::map<String, bool> gpioStates = {{"red", false}, {"green", false}};

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int speed = 0; // 現在の速度
}

#pragma region functions
// ledcでPFM
void pfmWrite(uint32_t freq) {
    ledcSetup(LEDC_CHANNEL_0, freq, LEDC_TIMER_12_BIT);
    ledcWrite(LEDC_CHANNEL_0, 2048); // 50%
    log_d("set freq: %d", freq);
}

// 速度で周波数を指定
void speedWrite(int16_t speed) {
    pfmWrite(speed * 6.472 + 5.070);
    log_d("set speed: %d", speed);
}
#pragma endregion

#pragma region web socket functions
void notifyAllClients() {
    JsonDocument doc;
    doc["type"] = "state";
    JsonObject states = doc["states"].to<JsonObject>();
    for (auto &kv : gpioStates) {
        states[kv.first] = kv.second;
    }
    doc["speed"] = speed;
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

void onWebSocketMessage(AsyncWebSocket *server, AsyncWebSocketClient *client,
                        AwsFrameInfo *info, char *data, size_t len,
                        AwsFrameType type, bool fin) {
    if (type == WS_TEXT) {
        JsonDocument doc;
        deserializeJson(doc, data);
        String id = doc["id"];
        String typeStr = doc["type"];

        if (typeStr == "toggle" && switchMap.count(id)) {
            gpioStates[id] = !gpioStates[id];
            digitalWrite(switchMap[id], gpioStates[id] ? HIGH : LOW);
        } else if (typeStr == "speed") {
            int s = doc["value"];
            if (s < 0) {
                s = 0; // 最小値
            } else if (s > 180) {
                s = 180; // 最大値
            }
            speed = s;
            speedWrite(s);
        }
        notifyAllClients();
    }
}
#pragma endregion

#pragma region web server functions
// 404
void notFound(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
        request->send(200);
    } else {
        request->send(404, "text/plain", "File not found");
    }
}
#pragma endregion

#pragma region setup and loop
void manual() {
    if (digitalRead(pin_btn)) {
        digitalWrite(pin_ledR, HIGH);
    } else {
        digitalWrite(pin_ledR, LOW);
    }
    uint16_t duty = ~analogRead(pin_ain) & 0x0fff;
    ledcWrite(LEDC_CHANNEL_1, duty);
    uint8_t speed = duty * 180 / 4096;
    speedWrite(speed);
}

void setup() {
    // pin
    pinMode(pin_ain, INPUT);
    pinMode(pin_btn, INPUT_PULLDOWN);
    pinMode(pin_ledG, OUTPUT);
    pinMode(pin_ledR, OUTPUT);
    pinMode(pin_out, OUTPUT);

    // serial
    Serial.begin(115200);
    Serial.println();
    log_d("setup");

    // wifi
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.softAP(ssid);
    // WiFi.softAP(ssid, password);
    delay(100);
    WiFi.softAPConfig(ip, ip, subnet);
    WiFi.begin();
    // WiFi.begin("SSID", "password"); // 初回は手動で設定
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(10);
    }
    Serial.println("connected to WiFi");
    log_d("IP address: %s", WiFi.localIP().toString().c_str());
    log_d("wifi end");

    // mdns
    while (!MDNS.begin(hostname)) {
        delay(10);
    }
    log_d("mdns end: %s.local", hostname);

    // output
    ledcSetup(LEDC_CHANNEL_1, 1000, LEDC_TIMER_12_BIT);
    ledcAttachPin(pin_ledG, LEDC_CHANNEL_1);
    ledcWrite(LEDC_CHANNEL_1, 0);
    log_d("output end");

    // WebSocket設定
    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                  AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_DATA) {
            onWebSocketMessage(server, client, (AwsFrameInfo *)arg,
                               (char *)data, len, WS_TEXT, true);
        } else if (type == WS_EVT_CONNECT) {
            notifyAllClients();
        }
    });
    server.addHandler(&ws);

    // SPIFFS設定
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
        log_e("SPIFFS Mount Failed");
        esp_restart();
    }
    SPIFFS.begin();
    log_d("SPIFFS mounted");

    // サーバー処理
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/test.html", "text/html");
    });
    server.serveStatic("/", SPIFFS, "/www/").setDefaultFile("test.html");
    server.onNotFound(notFound);

    // スタート
    server.begin();
    log_d("web server started");

    log_d("setup end");
}

void loop() {
    if (Serial.available()) {
        String str = Serial.readStringUntil('\n');
        speedWrite(str.toInt());
        log_d("input: %s", str.c_str());
    }
    // manual();

    delay(100);
}
#pragma endregion
