#include <WiFi.h>
#include <tinytuya.h>

const char *SSID = "your_wifi";
const char *PASS = "your_password";

struct DeviceConfig {
    const char *id;
    const char *ip;
    const char *key;
    float version;
};

DeviceConfig devices[] = {
    {"DEVICE_ID_1", "192.168.1.42", "0123456789abcdef", 3.3},
    {"DEVICE_ID_2", "192.168.1.43", "fedcba9876543210", 3.4},
};

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    for (const auto &cfg : devices) {
        TinyTuya dev(cfg.id, cfg.ip, cfg.key);
        dev.setVersion(cfg.version);
        String status;
        tuya_err_t err = dev.status(status);
        Serial.printf("%s -> %d %s\n", cfg.id, (int)err, err == TUYA_OK ? status.c_str() : "");
    }
}

void loop() {
}
