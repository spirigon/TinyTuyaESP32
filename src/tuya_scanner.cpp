#include "tuya_scanner.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <string.h>

#include "tuya_crypto.h"
#include "tuya_json.h"
#include "tuya_protocol.h"

static bool looks_json(const uint8_t *p, size_t len) {
    return p && len >= 2U && p[0] == '{';
}

static void trim_zeros(uint8_t *p, size_t *len) {
    if (!p || !len) return;
    while (*len > 0U && p[*len - 1U] == 0) --(*len);
}

static tuya_err_t decrypt_udp_packet(const uint8_t *packet, size_t packet_len,
                                     char *json, size_t json_len) {
    if (!packet || !json || json_len == 0) return TUYA_ERR_INVAL;
    if (looks_json(packet, packet_len)) {
        if (packet_len + 1U > json_len) return TUYA_ERR_BUFFER_TOO_SMALL;
        memcpy(json, packet, packet_len);
        json[packet_len] = '\0';
        return TUYA_OK;
    }

    uint8_t udp_key[16];
    tuya_crypto_udp_key(udp_key);

    tuya_header_t header;
    tuya_err_t err = tuya_protocol_parse_header(packet, packet_len, &header);
    uint8_t payload[TUYA_MAX_PAYLOAD_LENGTH + 1U];
    size_t payload_len = sizeof(payload) - 1U;
    tuya_message_t msg;

    if (err == TUYA_OK && header.prefix == TUYA_PREFIX_55AA) {
        err = tuya_protocol_unpack(packet, packet_len, nullptr, payload, &payload_len, &msg);
        if (err != TUYA_OK) return err;
        if (!looks_json(payload, payload_len)) {
            size_t dec_len = sizeof(payload) - 1U;
            err = tuya_crypto_aes_ecb_decrypt_pkcs7(udp_key, payload, payload_len, payload, &dec_len);
            if (err != TUYA_OK) return err;
            payload_len = dec_len;
        }
    } else if (err == TUYA_OK && header.prefix == TUYA_PREFIX_6699) {
        err = tuya_protocol_unpack(packet, packet_len, udp_key, payload, &payload_len, &msg);
        if (err != TUYA_OK) return err;
        trim_zeros(payload, &payload_len);
    } else {
        size_t dec_len = sizeof(payload) - 1U;
        err = tuya_crypto_aes_ecb_decrypt_pkcs7(udp_key, packet, packet_len, payload, &dec_len);
        if (err != TUYA_OK) return err;
        payload_len = dec_len;
    }

    if (!looks_json(payload, payload_len)) return TUYA_ERR_JSON;
    if (payload_len + 1U > json_len) return TUYA_ERR_BUFFER_TOO_SMALL;
    memcpy(json, payload, payload_len);
    json[payload_len] = '\0';
    return TUYA_OK;
}

static void send_v35_discovery(WiFiUDP &udp) {
    uint8_t key[16];
    tuya_crypto_udp_key(key);

    IPAddress local = WiFi.localIP();
    char body[64];
    snprintf(body, sizeof(body), "{\"from\":\"app\",\"ip\":\"%u.%u.%u.%u\"}",
             local[0], local[1], local[2], local[3]);

    uint8_t frame[256];
    size_t frame_len = sizeof(frame);
    if (tuya_protocol_pack_6699(0, TUYA_CMD_REQ_DEVINFO,
                                reinterpret_cast<const uint8_t *>(body),
                                strlen(body), key, nullptr, frame, &frame_len) != TUYA_OK) {
        return;
    }

    udp.beginPacket(IPAddress(255, 255, 255, 255), TUYA_UDP_PORT_APP);
    udp.write(frame, frame_len);
    udp.endPacket();
}

static bool seen_before(char seen[][32], size_t *seen_count, const char *gw_id) {
    if (!gw_id || !gw_id[0]) return false;
    for (size_t i = 0; i < *seen_count; ++i) {
        if (strncmp(seen[i], gw_id, 32U) == 0) return true;
    }
    if (*seen_count < 32U) {
        strncpy(seen[*seen_count], gw_id, 31U);
        seen[*seen_count][31] = '\0';
        ++(*seen_count);
    }
    return false;
}

static void poll_udp(WiFiUDP &udp,
                     tuya_device_found_cb_t callback,
                     void *user,
                     char seen[][32],
                     size_t *seen_count) {
    int packet_size = udp.parsePacket();
    if (packet_size <= 0) return;
    if (packet_size > TUYA_MAX_FRAME_LENGTH) {
        while (udp.available()) udp.read();
        return;
    }

    uint8_t packet[TUYA_MAX_FRAME_LENGTH];
    int len = udp.read(packet, sizeof(packet));
    if (len <= 0) return;

    char json[TUYA_MAX_JSON_LENGTH];
    if (decrypt_udp_packet(packet, (size_t)len, json, sizeof(json)) != TUYA_OK) return;

    tuya_device_info_t info;
    IPAddress ip = udp.remoteIP();
    char ip_text[16];
    snprintf(ip_text, sizeof(ip_text), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    if (tuya_json_parse_discovery(json, ip_text, &info) != TUYA_OK) return;
    if (seen_before(seen, seen_count, info.gw_id)) return;
    if (callback) callback(&info, user);
}

tuya_err_t tuya_scanner_scan(uint16_t seconds,
                             tuya_device_found_cb_t callback,
                             void *user) {
    if (WiFi.status() != WL_CONNECTED) return TUYA_ERR_OFFLINE;
    if (seconds == 0) seconds = TUYA_DISCOVERY_SECONDS;

    WiFiUDP udp31;
    WiFiUDP udp33;
    WiFiUDP udpapp;
    bool ok31 = udp31.begin(TUYA_UDP_PORT_31);
    bool ok33 = udp33.begin(TUYA_UDP_PORT_33);
    bool okapp = udpapp.begin(TUYA_UDP_PORT_APP);
    if (!ok31 && !ok33 && !okapp) return TUYA_ERR_CONNECT;

    char seen[32][32];
    memset(seen, 0, sizeof(seen));
    size_t seen_count = 0;

    uint32_t end = millis() + (uint32_t)seconds * 1000UL;
    uint32_t next_solicit = 0;
    while ((int32_t)(millis() - end) < 0) {
        if ((int32_t)(millis() - next_solicit) >= 0) {
            if (okapp) send_v35_discovery(udpapp);
            next_solicit = millis() + 6000UL;
        }
        if (ok31) poll_udp(udp31, callback, user, seen, &seen_count);
        if (ok33) poll_udp(udp33, callback, user, seen, &seen_count);
        if (okapp) poll_udp(udpapp, callback, user, seen, &seen_count);
        delay(5);
    }

    udp31.stop();
    udp33.stop();
    udpapp.stop();
    return TUYA_OK;
}
