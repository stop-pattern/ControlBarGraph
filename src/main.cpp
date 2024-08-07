#include <Arduino.h>
#include <WiFi.h>
#include <driver/ledc.h>

// mode select
#define SPEED
// - SPEED
//    表示する速度から周波数を計算して出力
// - FREQ
//    周波数を直接指定

constexpr uint8_t pin = 27;
const char* ssid = "BarGraqh";
const char* password = "password";
const IPAddress ip(192, 168, 0, 1);
const IPAddress subnet(255, 255, 255, 0);


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
    Serial.begin(115200);
    Serial.println();
    log_d("setup");

    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP(ssid, password);
    delay(100);
    WiFi.softAPConfig(ip, ip, subnet);
    
    pinMode(pin, OUTPUT);
    ledcAttachPin(pin, LEDC_CHANNEL_0);
    
#ifdef SPEED
        speedWrite(100);
#elif defined FREQ
        pfmWrite(1000);
#else
        log_d("mode not set");
#endif
}

// 速度指定
void loop() {
    if(Serial.available()){
        String str = Serial.readStringUntil('\n');
#ifdef SPEED
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
    delay(5);
}
