#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

const char *SSID = TINYTUYA_WIFI_SSID;
const char *PASS = TINYTUYA_WIFI_PASS;

struct DeviceConfig {
    const char *id;
    const char *ip;
    const char *key;
    float version;
    uint32_t pollMs;
};

DeviceConfig devices[] = {
    {TINYTUYA_DEVICE_ID, TINYTUYA_DEVICE_IP, TINYTUYA_LOCAL_KEY, TINYTUYA_PROTOCOL_VERSION, 30000UL},
#ifdef TINYTUYA_DEVICE_ID_2
    {TINYTUYA_DEVICE_ID_2, TINYTUYA_DEVICE_IP_2, TINYTUYA_LOCAL_KEY_2, TINYTUYA_PROTOCOL_VERSION_2, 30000UL},
#endif
#ifdef TINYTUYA_DEVICE_ID_3
    {TINYTUYA_DEVICE_ID_3, TINYTUYA_DEVICE_IP_3, TINYTUYA_LOCAL_KEY_3, TINYTUYA_PROTOCOL_VERSION_3, 30000UL},
#endif
#ifdef TINYTUYA_DEVICE_ID_4
    {TINYTUYA_DEVICE_ID_4, TINYTUYA_DEVICE_IP_4, TINYTUYA_LOCAL_KEY_4, TINYTUYA_PROTOCOL_VERSION_4, 30000UL},
#endif
};

TinyTuyaMulti tuya(sizeof(devices) / sizeof(devices[0]));

static void onTuyaEvent(size_t index,
                        const char *deviceId,
                        tuya_event_t event,
                        const char *payload,
                        tuya_err_t err,
                        void *user) {
    (void)user;
    Serial.printf("[%u] %s event=%d err=%d\n",
                  (unsigned)index, deviceId ? deviceId : "", (int)event, (int)err);
    if (payload) Serial.println(payload);
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    for (const auto &cfg : devices) {
        int index = tuya.addDevice(cfg.id, cfg.ip, cfg.key, cfg.version, cfg.pollMs);
        Serial.printf("add %s -> %d\n", cfg.id, index);
    }
    tuya.setCallback(onTuyaEvent);
}

void loop() {
    tuya.loop();
}
