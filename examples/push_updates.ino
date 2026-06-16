#include <WiFi.h>
#include <tinytuya.h>

const char *SSID = "your_wifi";
const char *PASS = "your_password";

TinyTuya dev("DEVICE_ID_HERE", "192.168.1.42", "0123456789abcdef");
uint32_t nextHeartbeat = 0;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    dev.setVersion(3.3);
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
