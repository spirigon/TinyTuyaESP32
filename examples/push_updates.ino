#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

TinyTuya dev(TINYTUYA_DEVICE_ID, TINYTUYA_DEVICE_IP, TINYTUYA_LOCAL_KEY);
uint32_t nextHeartbeat = 0;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    dev.setVersion(TINYTUYA_PROTOCOL_VERSION);
    dev.setPersistent(true);

    String status;
    tuya_err_t err = dev.status(status);
    Serial.printf("Initial status: %d %s\n", (int)err, err == TUYA_OK ? status.c_str() : "");
}

void loop() {
    if ((int32_t)(millis() - nextHeartbeat) >= 0) {
        tuya_err_t err = dev.heartbeat();
        Serial.printf("Heartbeat: %d\n", (int)err);
        nextHeartbeat = millis() + 30000UL;
    }
}
