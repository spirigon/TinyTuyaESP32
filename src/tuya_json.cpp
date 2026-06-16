#include "tuya_json.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <ArduinoJson.h>

static long now_seconds() {
#if defined(ARDUINO)
    time_t now = time(nullptr);
    if (now > 1600000000) return (long)now;
    return (long)(millis() / 1000UL);
#else
    return 0;
#endif
}

static tuya_err_t check_snprintf(int n, size_t cap) {
    if (n < 0) return TUYA_ERR_PAYLOAD;
    return (size_t)n < cap ? TUYA_OK : TUYA_ERR_BUFFER_TOO_SMALL;
}

tuya_err_t tuya_json_build_status(const tuya_device_t *dev,
                                  tuya_cmd_t *cmd,
                                  char *out,
                                  size_t out_len) {
    if (!dev || !cmd || !out || out_len == 0) return TUYA_ERR_INVAL;
    if (dev->version >= TUYA_PROTO_34) {
        *cmd = TUYA_CMD_DP_QUERY_NEW;
        int n = snprintf(out, out_len, "{}");
        return check_snprintf(n, out_len);
    }

    if (dev->is_device22 || dev->version == TUYA_PROTO_32) {
        *cmd = TUYA_CMD_CONTROL_NEW;
        int n = snprintf(out, out_len,
                         "{\"devId\":\"%s\",\"uid\":\"%s\",\"t\":\"%ld\",\"dps\":{\"1\":null}}",
                         dev->id, dev->id, now_seconds());
        return check_snprintf(n, out_len);
    }

    *cmd = TUYA_CMD_DP_QUERY;
    int n = snprintf(out, out_len,
                     "{\"gwId\":\"%s\",\"devId\":\"%s\",\"uid\":\"%s\",\"t\":\"%ld\"}",
                     dev->id, dev->id, dev->id, now_seconds());
    return check_snprintf(n, out_len);
}

tuya_err_t tuya_json_build_set(const tuya_device_t *dev,
                               uint8_t dp,
                               const char *value_json,
                               tuya_cmd_t *cmd,
                               char *out,
                               size_t out_len) {
    if (!dev || !value_json || !cmd || !out || out_len == 0) return TUYA_ERR_INVAL;
    if (dev->version >= TUYA_PROTO_34) {
        *cmd = TUYA_CMD_CONTROL_NEW;
        int n = snprintf(out, out_len,
                         "{\"protocol\":5,\"t\":%ld,\"data\":{\"dps\":{\"%u\":%s}}}",
                         now_seconds(), (unsigned)dp, value_json);
        return check_snprintf(n, out_len);
    }

    *cmd = TUYA_CMD_CONTROL;
    int n = snprintf(out, out_len,
                     "{\"devId\":\"%s\",\"uid\":\"%s\",\"t\":\"%ld\",\"dps\":{\"%u\":%s}}",
                     dev->id, dev->id, now_seconds(), (unsigned)dp, value_json);
    return check_snprintf(n, out_len);
}

tuya_err_t tuya_json_build_heartbeat(const tuya_device_t *dev,
                                     tuya_cmd_t *cmd,
                                     char *out,
                                     size_t out_len) {
    if (!dev || !cmd || !out || out_len == 0) return TUYA_ERR_INVAL;
    *cmd = TUYA_CMD_HEART_BEAT;
    int n = snprintf(out, out_len, "{\"gwId\":\"%s\",\"devId\":\"%s\"}", dev->id, dev->id);
    return check_snprintf(n, out_len);
}

tuya_err_t tuya_json_validate(const char *json) {
    if (!json) return TUYA_ERR_INVAL;
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    StaticJsonDocument<1024> doc;
#endif
    DeserializationError err = deserializeJson(doc, json);
    return err ? TUYA_ERR_JSON : TUYA_OK;
}

static void copy_json_string(JsonVariantConst value, char *dst, size_t dst_len) {
    if (!dst || dst_len == 0) return;
    const char *s = value.is<const char *>() ? value.as<const char *>() : "";
    strncpy(dst, s ? s : "", dst_len - 1U);
    dst[dst_len - 1U] = '\0';
}

tuya_err_t tuya_json_parse_discovery(const char *json,
                                     const char *ip,
                                     tuya_device_info_t *info) {
    if (!json || !info) return TUYA_ERR_INVAL;
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 7
    JsonDocument doc;
#else
    StaticJsonDocument<1024> doc;
#endif
    DeserializationError err = deserializeJson(doc, json);
    if (err) return TUYA_ERR_JSON;

    memset(info, 0, sizeof(*info));
    if (ip) {
        strncpy(info->ip, ip, sizeof(info->ip) - 1U);
    }

    JsonVariantConst gw = doc["gwId"];
    if (gw.isNull()) gw = doc["id"];
    copy_json_string(gw, info->gw_id, sizeof(info->gw_id));
    copy_json_string(doc["productKey"], info->product_key, sizeof(info->product_key));

    JsonVariantConst ver = doc["version"];
    if (ver.is<const char *>()) {
        copy_json_string(ver, info->version, sizeof(info->version));
    } else if (ver.is<float>() || ver.is<double>()) {
        snprintf(info->version, sizeof(info->version), "%.1f", ver.as<double>());
    } else if (ver.is<int>()) {
        snprintf(info->version, sizeof(info->version), "%d", ver.as<int>());
    }

    strncpy(info->raw_json, json, sizeof(info->raw_json) - 1U);
    info->raw_json[sizeof(info->raw_json) - 1U] = '\0';
    return info->gw_id[0] ? TUYA_OK : TUYA_ERR_JSON;
}
