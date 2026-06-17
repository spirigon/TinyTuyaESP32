#include <WiFi.h>
#include <tinytuya.h>
#include "example_wifi_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

const char *DEVICE_ID = "DEVICE_ID_HERE";
const char *DEVICE_IP = "192.168.1.42";
const char *LOCAL_KEY = "0123456789abcdef";
const float PROTOCOL_VERSION = 3.3;
const uint8_t RELAY_DP = 1;

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
