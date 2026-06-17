#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("TinyTuyaESP32 board smoke test");
    Serial.printf("Chip model: %s\n", ESP.getChipModel());
    Serial.printf("CPU MHz: %u\n", ESP.getCpuFreqMHz());
    Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
}

void loop() {
    Serial.printf("alive millis=%lu free_heap=%u\n",
                  (unsigned long)millis(),
                  ESP.getFreeHeap());
    delay(1000);
}
