#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

TinyTuyaAsync tuya;
uint32_t nextPoll = 0;

static void onTuya(tuya_event_t event, const char *payload, void *user) {
    (void)user;
    if (event == TUYA_EVENT_STATUS_RECEIVED) {
        Serial.println(payload ? payload : "{}");
    } else if (event == TUYA_EVENT_ERROR) {
        Serial.printf("Tuya async error: %d\n", (int)tuya.lastError());
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    tuya_err_t err = tuya.begin(TINYTUYA_DEVICE_ID,
                                TINYTUYA_DEVICE_IP,
                                TINYTUYA_LOCAL_KEY,
                                TINYTUYA_PROTOCOL_VERSION,
                                onTuya);
    Serial.printf("TinyTuyaAsync begin: %d\n", (int)err);
    if (err == TUYA_OK) tuya.requestStatus();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        delay(1000);
        return;
    }

    if ((int32_t)(millis() - nextPoll) >= 0 && !tuya.busy()) {
        tuya.requestStatus();
        nextPoll = millis() + 30000UL;
    }
}
