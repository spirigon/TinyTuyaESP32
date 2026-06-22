#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

TinyTuya dev(TINYTUYA_DEVICE_ID, TINYTUYA_DEVICE_IP, TINYTUYA_LOCAL_KEY);
uint32_t nextPoll = 0;

static bool connectWifi(uint32_t timeoutMs) {
    Serial.printf("Connecting WiFi SSID=%s\n", SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("WiFi failed: status=%d\n", (int)WiFi.status());
        return false;
    }

    Serial.print("WiFi connected: ");
    Serial.println(WiFi.localIP());
    return true;
}

static void pollTuya() {
    String status;
    uint32_t start = millis();
    tuya_err_t err = dev.status(status);
    uint32_t elapsed = millis() - start;

    Serial.printf("tuya_status err=%d elapsed_ms=%lu\n", (int)err, (unsigned long)elapsed);
    if (err == TUYA_OK) {
        Serial.println(status);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("TinyTuyaESP32 validate_device");
    Serial.printf("Target ip=%s protocol=%.1f\n", TINYTUYA_DEVICE_IP, TINYTUYA_PROTOCOL_VERSION);

    if (!connectWifi(20000UL)) return;

    dev.setVersion(TINYTUYA_PROTOCOL_VERSION);
    pollTuya();
    nextPoll = millis() + 10000UL;
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected; reconnecting");
        connectWifi(20000UL);
    }

    if ((int32_t)(millis() - nextPoll) >= 0) {
        pollTuya();
        nextPoll = millis() + 10000UL;
    }
}
