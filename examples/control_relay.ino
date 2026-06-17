#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

const char *DEVICE_ID = TINYTUYA_DEVICE_ID;
const char *DEVICE_IP = TINYTUYA_DEVICE_IP;
const char *LOCAL_KEY = TINYTUYA_LOCAL_KEY;
const float PROTOCOL_VERSION = TINYTUYA_PROTOCOL_VERSION;
const uint8_t RELAY_DP = TINYTUYA_RELAY_DP;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    TinyTuya dev(DEVICE_ID, DEVICE_IP, LOCAL_KEY);
    dev.setVersion(PROTOCOL_VERSION);

    tuya_err_t err = dev.setBool(RELAY_DP, true);
    Serial.printf("Relay on: %d\n", (int)err);
    delay(2000);
    err = dev.setBool(RELAY_DP, false);
    Serial.printf("Relay off: %d\n", (int)err);
}

void loop() {
}
