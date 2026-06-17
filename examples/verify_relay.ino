#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

TinyTuya dev(TINYTUYA_DEVICE_ID, TINYTUYA_DEVICE_IP, TINYTUYA_LOCAL_KEY);

static void printStatus(const char *label) {
    String status;
    tuya_err_t err = dev.status(status);
    Serial.printf("%s status: %d", label, (int)err);
    if (err == TUYA_OK) {
        Serial.print(" ");
        Serial.print(status);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    dev.setVersion(TINYTUYA_PROTOCOL_VERSION);

    tuya_err_t err = dev.setBool(TINYTUYA_RELAY_DP, true);
    Serial.printf("Relay on command: %d\n", (int)err);
    delay(1500);
    printStatus("After on");

    err = dev.setBool(TINYTUYA_RELAY_DP, false);
    Serial.printf("Relay off command: %d\n", (int)err);
    delay(1500);
    printStatus("After off");
}

void loop() {
}
