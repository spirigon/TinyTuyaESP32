#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

TinyTuyaFsm fsm;
uint32_t nextPoll = 0;

static void onEvent(tuya_event_t event, const char *payload, void *user) {
    (void)user;
    if (event == TUYA_EVENT_STATUS_RECEIVED && payload) {
        Serial.println(payload);
    } else if (event == TUYA_EVENT_ERROR) {
        Serial.println("Tuya error");
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    fsm.begin(TINYTUYA_DEVICE_ID, TINYTUYA_DEVICE_IP, TINYTUYA_LOCAL_KEY,
              TINYTUYA_PROTOCOL_VERSION, onEvent);
    fsm.requestStatus();
}

void loop() {
    fsm.loop();
    if ((int32_t)(millis() - nextPoll) >= 0) {
        fsm.requestStatus();
        nextPoll = millis() + 30000UL;
    }
}
