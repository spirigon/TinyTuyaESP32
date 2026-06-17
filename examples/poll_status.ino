#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

const char *DEVICE_ID = TINYTUYA_DEVICE_ID;
const char *DEVICE_IP = TINYTUYA_DEVICE_IP;
const char *LOCAL_KEY = TINYTUYA_LOCAL_KEY;
const float PROTOCOL_VERSION = TINYTUYA_PROTOCOL_VERSION;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    TinyTuya dev(DEVICE_ID, DEVICE_IP, LOCAL_KEY);
    dev.setVersion(PROTOCOL_VERSION);

    String status;
    tuya_err_t err = dev.status(status);
    if (err == TUYA_OK) {
        Serial.println(status);
    } else {
        Serial.printf("tuya_status failed: %d\n", (int)err);
    }
}

void loop() {
}
