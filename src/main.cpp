#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <driver/ledc.h>
#include "FS.h"
#include "SPIFFS.h"

// mode select
//#define SERVER
// - SERVER
//    サーバーから表示する速度を受け周波数を計算して出力
// - SPEED
//    表示する速度から周波数を計算して出力
// - FREQ
//    周波数を直接指定

# define FORMAT_SPIFFS_IF_FAILED true

constexpr uint8_t pin_ain = 39;     // 可変抵抗
constexpr uint8_t pin_ledR = 26;    // LED緑
constexpr uint8_t pin_ledG = 25;    // LED赤
constexpr uint8_t pin_btn = 13;     // タクトスイッチ
constexpr uint8_t pin_out = 4;      // PFM出力
// constexpr uint8_t pin_out = 2;      // 試作時出力

const char* ssid = "BarGraqh";
const char* password = "password";
const IPAddress ip(192, 168, 0, 1);
const IPAddress subnet(255, 255, 255, 0);
const char* hostname = "esp32";

#ifdef SERVER
AsyncWebServer server(80);
#endif

bool isServerEnable = true;
volatile ulong preTime = 0;
volatile bool intrStatus = false;

// ledcでPFM
void pfmWrite(uint32_t freq) {
    ledcSetup(LEDC_CHANNEL_0, freq, LEDC_TIMER_12_BIT);
    ledcWrite(LEDC_CHANNEL_0, 2048);// 50%
    log_d("set freq: %d", freq);
}

// 速度で周波数を指定
void speedWrite(int16_t speed) {
    pfmWrite(speed * 6.472 + 5.070);
    log_d("set speed: %d", speed);
}

// 404
void notFound(AsyncWebServerRequest *request){
    if (request->method() == HTTP_OPTIONS){
        request->send(200);
    }else{
        request->send(404, "text/plain", "File not found");
    }
}

void manual(){
    uint16_t duty = ~analogRead(pin_ain) & 0x0fff;
    // ledcWrite(LEDC_CHANNEL_1, duty);
    uint8_t speed = duty * 180 / 4096;
    speedWrite(speed);
}

// 割り込み処理
void pinChanged() {
    // チャタリング除去
    if (millis() - preTime < 10) {
        return;
    }
    intrStatus = true;
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
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(ssid, password);
    delay(100);
    WiFi.softAPConfig(ip, ip, subnet);
    log_d("wifi end");

    // mdns   
    while (!MDNS.begin(hostname)) {
        delay(10);
    }
    log_d("mdns end");

    // input
    attachInterrupt(digitalPinToInterrupt(pin_btn), pinChanged, RISING);

    // output
    ledcAttachPin(pin_out, LEDC_CHANNEL_0);
    log_d("output end");

    // mode options
#if defined SERVER
    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        log_e("SPIFFS Mount Failed");
        esp_restart();
    }
    SPIFFS.begin();
    server.serveStatic("/favicon.ico", SPIFFS, "/favicon.ico"); //配置せずdefault
    server.serveStatic("/Chart.min.js", SPIFFS, "/Chart.min.js");
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", String(), false);
    });
    // server.on("/frequency", HTTP_GET, [](AsyncWebServerRequest *request){
    //     request->send_P(200, "text/plain", getFrequency().c_str());
    // });
    // server.on("/speed", HTTP_GET, [](AsyncWebServerRequest *request){
    //     request->send_P(200, "text/plain", getSpeed).c_str());
    // });
    server.serveStatic("/", SPIFFS, "/www/").setDefaultFile("index.html");
    server.onNotFound(notFound);
    server.begin();
    MDNS.addService("server", "tcp", 80);
#elif defined SPEED
    speedWrite(100);
#elif defined FREQ
    pfmWrite(1000);
#else
    log_d("mode not set");
#endif
    // ledcSetup(LEDC_CHANNEL_1, 1000, LEDC_TIMER_12_BIT);
    // ledcAttachPin(pin_ledG, LEDC_CHANNEL_1);
    // ledcWrite(LEDC_CHANNEL_1, 0);
    digitalWrite(pin_ledG, HIGH);
    digitalWrite(pin_ledR, isServerEnable);
    log_d("setup end");
}

void loop() {
#if not defined SERVER
    if(Serial.available()){
        String str = Serial.readStringUntil('\n');
#if defined SPEED
        speedWrite(str.toInt());
#elif defined FREQ
        uint32_t freq = str.toInt();
        if (freq == 0) {
            digitalWrite(pin, LOW);
            log_d("set Low");
        }
        else{
            prmWrite(freq);
        }
#else
        log_d("input: %s", str.c_str());
#endif
    }
#endif
    if (intrStatus) {
        isServerEnable = !isServerEnable;
        intrStatus = false;
    }
    digitalWrite(pin_ledR, isServerEnable);

    if (!isServerEnable) {
        manual();
    }
    else {
        speedWrite(180);
        ulong temp = millis();
        while (millis() - temp >= 37500) {
            if (intrStatus) return;
            delay(1);
        }
        delay(1000);
        speedWrite(0);
        temp = millis();
        while (millis() - temp >= 37500) {
            if (intrStatus) return;
            delay(1);
        }
        delay(1000);
    }

    delay(100);
}
