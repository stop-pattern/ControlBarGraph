#include <Arduino.h>
#include <driver/ledc.h>

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
}

void setup() {
    Serial.begin(115200);
    Serial.println("setup");
    
    pinMode(pin, OUTPUT);
    
    ledcAttachPin(pin, LEDC_CHANNEL_0);
    pfmWrite(1000);
}

void loop() {
    if(Serial.available()){
        String str = Serial.readStringUntil('\n');
        uint32_t freq = str.toInt();
        if (freq == 0) {
            digitalWrite(pin, LOW);
        }
        else{
            pfmWrite(freq);
        }
        Serial.print("set freq: ");
        Serial.println(freq);
    }
    delay(5);
}
