#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <driver/ledc.h>

// mode select
#define SERVER
// - SERVER
//    サーバーから表示する速度を受け周波数を計算して出力
// - SPEED
//    表示する速度から周波数を計算して出力
// - FREQ
//    周波数を直接指定

constexpr uint8_t pin = 27;
const char* ssid = "BarGraqh";
const char* password = "password";
const IPAddress ip(192, 168, 0, 1);
const IPAddress subnet(255, 255, 255, 0);
const char* hostname = "esp32";

#ifdef SERVER
WebServer Server(80);
#endif

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

void setup() {
    // serial
    Serial.begin(115200);
    Serial.println();
    log_d("setup");

    // wifi
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(ssid, password);
    delay(100);
    WiFi.softAPConfig(ip, ip, subnet);

    // mdns   
    while (!MDNS.begin(hostname)) {
        delay(10);
    }

    // output
    pinMode(pin, OUTPUT);
    ledcAttachPin(pin, LEDC_CHANNEL_0);

    // mode options
#if defined SERVER
    Server.begin();
    MDNS.addService("server", "tcp", 80);
#elif defined SPEED
    speedWrite(100);
#elif defined FREQ
    pfmWrite(1000);
#else
    log_d("mode not set");
#endif
}

void loop() {
#if defined SERVER
    Server.handleClient();
#else
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
    delay(5);
}
