#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

#ifndef TINYTUYA_FORWARD_SERVER_URL
#define TINYTUYA_FORWARD_SERVER_URL ""
#endif

#ifndef TINYTUYA_FORWARD_INTERVAL_MS
#define TINYTUYA_FORWARD_INTERVAL_MS 30000UL
#endif

static const char *SSID = TINYTUYA_WIFI_SSID;
static const char *PASS = TINYTUYA_WIFI_PASS;

TinyTuya dev(TINYTUYA_DEVICE_ID, TINYTUYA_DEVICE_IP, TINYTUYA_LOCAL_KEY);
uint32_t nextForwardMs = 0;

static bool connectWifi(uint32_t timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) return true;

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASS);

    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < timeoutMs) {
        delay(250);
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

static bool buildPayload(const String &statusJson,
                         tuya_err_t tuyaErr,
                         uint32_t pollLatencyMs,
                         String &out) {
    JsonDocument payload;
    payload["deviceId"] = TINYTUYA_DEVICE_ID;
    payload["ip"] = TINYTUYA_DEVICE_IP;
    payload["protocolVersion"] = TINYTUYA_PROTOCOL_VERSION;
    payload["tuyaError"] = (int)tuyaErr;
    payload["pollLatencyMs"] = pollLatencyMs;

    JsonObject diagnostics = payload["diagnostics"].to<JsonObject>();
    diagnostics["wifiRssi"] = WiFi.RSSI();
    diagnostics["freeHeap"] = ESP.getFreeHeap();
    diagnostics["uptimeMs"] = millis();

    if (statusJson.length() > 0) {
        JsonDocument statusDoc;
        DeserializationError parseErr = deserializeJson(statusDoc, statusJson);
        if (parseErr) {
            payload["statusParseError"] = parseErr.c_str();
            payload["rawStatus"] = statusJson;
        } else {
            payload["status"].set(statusDoc.as<JsonVariant>());
        }
    }

    out = "";
    serializeJson(payload, out);
    return out.length() > 0;
}

static bool postJson(const String &body) {
    if (TINYTUYA_FORWARD_SERVER_URL[0] == '\0') {
        Serial.println("Set FORWARD_SERVER_URL in .env to enable HTTP forwarding.");
        return false;
    }

    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(TINYTUYA_FORWARD_SERVER_URL)) {
        Serial.println("HTTP begin failed");
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();

    Serial.printf("HTTP POST code=%d bytes=%u\n", code, (unsigned)body.length());
    return code >= 200 && code < 300;
}

static void pollAndForward() {
    String status;
    uint32_t started = millis();
    tuya_err_t err = dev.status(status);
    uint32_t latency = millis() - started;

    Serial.printf("tuya_status err=%d latency_ms=%lu\n",
                  (int)err,
                  (unsigned long)latency);
    if (err == TUYA_OK) Serial.println(status);

    String body;
    if (buildPayload(status, err, latency, body)) {
        postJson(body);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("TinyTuyaESP32 generic HTTP forwarding example");
    if (!connectWifi(20000UL)) return;

    dev.setVersion(TINYTUYA_PROTOCOL_VERSION);
    pollAndForward();
    nextForwardMs = millis() + TINYTUYA_FORWARD_INTERVAL_MS;
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWifi(20000UL);
        delay(1000);
        return;
    }

    if ((int32_t)(millis() - nextForwardMs) >= 0) {
        pollAndForward();
        nextForwardMs = millis() + TINYTUYA_FORWARD_INTERVAL_MS;
    }
}
