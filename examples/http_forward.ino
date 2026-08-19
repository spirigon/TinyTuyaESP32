#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <tinytuya.h>
#include "example_tuya_config.h"

#ifndef TINYTUYA_FORWARD_SERVER_URL
#define TINYTUYA_FORWARD_SERVER_URL ""
#endif

#ifndef TINYTUYA_FORWARD_INTERVAL_MS
#define TINYTUYA_FORWARD_INTERVAL_MS 30000UL
#endif

#ifndef TINYTUYA_FORWARD_TLS_HOSTNAME
#define TINYTUYA_FORWARD_TLS_HOSTNAME ""
#endif

#ifndef TINYTUYA_FORWARD_GATEWAY_ID
#define TINYTUYA_FORWARD_GATEWAY_ID ""
#endif

#ifndef TINYTUYA_FORWARD_TOKEN
#define TINYTUYA_FORWARD_TOKEN ""
#endif

#ifndef TINYTUYA_FORWARD_CA_CERT
#define TINYTUYA_FORWARD_CA_CERT ""
#endif

static const char *SSID = TINYTUYA_WIFI_SSID;
static const char *PASS = TINYTUYA_WIFI_PASS;

struct DeviceConfig {
    const char *id;
    const char *ip;
    const char *key;
    float version;
};

static DeviceConfig devices[] = {
    {TINYTUYA_DEVICE_ID, TINYTUYA_DEVICE_IP, TINYTUYA_LOCAL_KEY, TINYTUYA_PROTOCOL_VERSION},
#ifdef TINYTUYA_DEVICE_ID_2
    {TINYTUYA_DEVICE_ID_2, TINYTUYA_DEVICE_IP_2, TINYTUYA_LOCAL_KEY_2, TINYTUYA_PROTOCOL_VERSION_2},
#endif
#ifdef TINYTUYA_DEVICE_ID_3
    {TINYTUYA_DEVICE_ID_3, TINYTUYA_DEVICE_IP_3, TINYTUYA_LOCAL_KEY_3, TINYTUYA_PROTOCOL_VERSION_3},
#endif
#ifdef TINYTUYA_DEVICE_ID_4
    {TINYTUYA_DEVICE_ID_4, TINYTUYA_DEVICE_IP_4, TINYTUYA_LOCAL_KEY_4, TINYTUYA_PROTOCOL_VERSION_4},
#endif
#ifdef TINYTUYA_DEVICE_ID_5
    {TINYTUYA_DEVICE_ID_5, TINYTUYA_DEVICE_IP_5, TINYTUYA_LOCAL_KEY_5, TINYTUYA_PROTOCOL_VERSION_5},
#endif
#ifdef TINYTUYA_DEVICE_ID_6
    {TINYTUYA_DEVICE_ID_6, TINYTUYA_DEVICE_IP_6, TINYTUYA_LOCAL_KEY_6, TINYTUYA_PROTOCOL_VERSION_6},
#endif
#ifdef TINYTUYA_DEVICE_ID_7
    {TINYTUYA_DEVICE_ID_7, TINYTUYA_DEVICE_IP_7, TINYTUYA_LOCAL_KEY_7, TINYTUYA_PROTOCOL_VERSION_7},
#endif
#ifdef TINYTUYA_DEVICE_ID_8
    {TINYTUYA_DEVICE_ID_8, TINYTUYA_DEVICE_IP_8, TINYTUYA_LOCAL_KEY_8, TINYTUYA_PROTOCOL_VERSION_8},
#endif
};

static TinyTuyaMulti tuya(sizeof(devices) / sizeof(devices[0]));

class ForwardTlsClient : public WiFiClientSecure {
   public:
    int connect(const char *host, uint16_t port) override {
        IPAddress endpointIp;
        if (endpointIp.fromString(host) &&
            TINYTUYA_FORWARD_TLS_HOSTNAME[0] != '\0') {
            return WiFiClientSecure::connect(
                endpointIp,
                port,
                TINYTUYA_FORWARD_TLS_HOSTNAME,
                TINYTUYA_FORWARD_CA_CERT,
                nullptr,
                nullptr);
        }
        return WiFiClientSecure::connect(host, port);
    }
};

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

static bool ensureClock(uint32_t timeoutMs) {
    constexpr time_t MIN_VALID_TIME = 1700000000;
    if (time(nullptr) >= MIN_VALID_TIME) return true;

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    uint32_t started = millis();
    while (time(nullptr) < MIN_VALID_TIME && millis() - started < timeoutMs) {
        delay(250);
    }
    if (time(nullptr) < MIN_VALID_TIME) {
        Serial.println("NTP sync failed; cannot validate HTTPS certificate");
        return false;
    }
    return true;
}

static bool buildPayload(const DeviceConfig &device,
                         size_t deviceIndex,
                         tuya_event_t event,
                         const char *statusJson,
                         tuya_err_t tuyaErr,
                         String &out) {
    JsonDocument payload;
    payload["gatewayId"] = TINYTUYA_FORWARD_GATEWAY_ID;
    payload["deviceId"] = device.id;
    payload["ip"] = device.ip;
    payload["protocolVersion"] = device.version;
    payload["event"] = event == TUYA_EVENT_ERROR ? "error" : "status";
    payload["tuyaError"] = (int)tuyaErr;

    JsonObject diagnostics = payload["diagnostics"].to<JsonObject>();
    diagnostics["deviceIndex"] = deviceIndex;
    diagnostics["wifiRssi"] = WiFi.RSSI();
    diagnostics["freeHeap"] = ESP.getFreeHeap();
    diagnostics["uptimeMs"] = millis();

    if (statusJson && statusJson[0] != '\0') {
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
        Serial.println("Set FORWARD_SERVER_URL in .env to enable HTTPS forwarding.");
        return false;
    }
    if (!String(TINYTUYA_FORWARD_SERVER_URL).startsWith("https://")) {
        Serial.println("FORWARD_SERVER_URL must use https://");
        return false;
    }
    if (TINYTUYA_FORWARD_GATEWAY_ID[0] == '\0') {
        Serial.println("Set FORWARD_GATEWAY_ID for gateway authentication.");
        return false;
    }
    if (TINYTUYA_FORWARD_CA_CERT[0] == '\0') {
        Serial.println("Set FORWARD_CA_CERT_FILE to the trusted root CA certificate.");
        return false;
    }
    if (TINYTUYA_FORWARD_TOKEN[0] == '\0') {
        Serial.println("Set FORWARD_TOKEN for X-ESP32-Token authentication.");
        return false;
    }
    if (!ensureClock(15000UL)) return false;

    ForwardTlsClient tlsClient;
    tlsClient.setCACert(TINYTUYA_FORWARD_CA_CERT);

    HTTPClient http;
    http.setTimeout(5000);
    if (!http.begin(tlsClient, TINYTUYA_FORWARD_SERVER_URL)) {
        Serial.println("HTTPS begin failed");
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-ESP32-Gateway", TINYTUYA_FORWARD_GATEWAY_ID);
    http.addHeader("X-ESP32-Token", TINYTUYA_FORWARD_TOKEN);
    int code = http.POST(body);
    if (code < 0) {
        char tlsError[128] = {};
        int tlsCode = tlsClient.lastError(tlsError, sizeof(tlsError));
        Serial.printf("HTTPS error: http=%s tls=%d (%s)\n",
                      HTTPClient::errorToString(code).c_str(),
                      tlsCode,
                      tlsError);
    }
    http.end();

    Serial.printf("HTTPS POST code=%d bytes=%u\n", code, (unsigned)body.length());
    return code >= 200 && code < 300;
}

static void onTuyaEvent(size_t index,
                        const char *deviceId,
                        tuya_event_t event,
                        const char *payload,
                        tuya_err_t err,
                        void *user) {
    (void)user;
    if (index >= sizeof(devices) / sizeof(devices[0])) return;

    Serial.printf("[%u] %s event=%d err=%d\n",
                  (unsigned)index,
                  deviceId ? deviceId : "",
                  (int)event,
                  (int)err);

    String body;
    if (buildPayload(devices[index], index, event, payload, err, body)) {
        postJson(body);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("TinyTuyaESP32 multi-device HTTPS forwarding example");
    if (!connectWifi(20000UL)) return;

    for (const auto &device : devices) {
        int index = tuya.addDevice(
            device.id,
            device.ip,
            device.key,
            device.version,
            TINYTUYA_FORWARD_INTERVAL_MS);
        Serial.printf("add %s -> %d\n", device.id, index);
    }
    tuya.setCallback(onTuyaEvent);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWifi(20000UL);
        delay(1000);
        return;
    }
    tuya.loop();
    delay(1);
}
