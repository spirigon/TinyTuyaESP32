#include <WiFi.h>
#include <tinytuya.h>

const char *SSID = "your_wifi";
const char *PASS = "your_password";

static void onDeviceFound(const tuya_device_info_t *info, void *user) {
    (void)user;
    Serial.printf("[FOUND] ip=%s gwId=%s version=%s product=%s\n",
                  info->ip, info->gw_id, info->version, info->product_key);
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) delay(250);

    Serial.println("Scanning for Tuya devices...");
    tuya_err_t err = TuyaScanner::scan(18, onDeviceFound);
    Serial.printf("Scan finished: %d\n", (int)err);
}

void loop() {
}
