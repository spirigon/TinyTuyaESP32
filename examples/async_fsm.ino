#include <WiFi.h>
#include <tinytuya.h>

const char *SSID = "your_wifi";
const char *PASS = "your_password";

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

    fsm.begin("DEVICE_ID_HERE", "192.168.1.42", "0123456789abcdef", 3.3, onEvent);
    fsm.requestStatus();
}

void loop() {
    fsm.loop();
    if ((int32_t)(millis() - nextPoll) >= 0) {
        fsm.requestStatus();
        nextPoll = millis() + 30000UL;
    }
}
