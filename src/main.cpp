#include <Arduino.h>
#include <driver/ledc.h>

// mode select
#define SPEED
// - SPEED
//    表示する速度から周波数を計算して出力
// - FREQ
//    周波数を直接指定

constexpr uint8_t pin = 27;

// 文字列がintに変換可能か
bool isInt(String str) {
    for (auto &&i : str) {
        // if (!isdigit(i)) return false;
        Serial.print(isdigit(i));
    }
    return true;
}

// ledcでPFM
void pfmWrite(uint32_t freq) {
    ledcSetup(LEDC_CHANNEL_0, freq, LEDC_TIMER_12_BIT);
    ledcWrite(LEDC_CHANNEL_0, 2048);// 50%
    Serial.printf("set freq: %d\n", freq);
}

// 速度で周波数を指定
void speedWrite(int16_t speed) {
    pfmWrite(speed * 6.472 + 5.070);
    Serial.printf("set speed: %d\n", speed);
}

void setup() {
    Serial.begin(115200);
    Serial.println("setup");
    
    pinMode(pin, OUTPUT);
    ledcAttachPin(pin, LEDC_CHANNEL_0);
    
#ifdef SPEED
        speedWrite(100);
#elif defined FREQ
        pfmWrite(1000);
#else
        Serial.println("mode not set");
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
            Serial.println("set Low");
        }
        else{
            prmWrite(freq);
        }
#else
        Serial.println("input: " + str);
#endif
    }
    delay(5);
}
