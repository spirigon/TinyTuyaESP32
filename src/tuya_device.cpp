#include "tuya_device.h"

#include <Arduino.h>
#include <WiFiClient.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tuya_crypto.h"
#include "tuya_json.h"
#include "tuya_log.h"
#include "tuya_protocol.h"
#include "tuya_session.h"

static const uint8_t VERSION_31[] = {'3', '.', '1'};

class HeapBuffer {
public:
    explicit HeapBuffer(size_t size) : data_(static_cast<uint8_t *>(malloc(size))), size_(size) {}
    ~HeapBuffer() { free(data_); }

    HeapBuffer(const HeapBuffer &) = delete;
    HeapBuffer &operator=(const HeapBuffer &) = delete;

    uint8_t *data() { return data_; }
    size_t size() const { return size_; }
    bool ok() const { return data_ != nullptr; }

private:
    uint8_t *data_;
    size_t size_;
};

static const uint8_t *real_key(const tuya_device_t *dev) {
    return reinterpret_cast<const uint8_t *>(dev->local_key);
}

static const uint8_t *active_key(const tuya_device_t *dev) {
    if (dev->version >= TUYA_PROTO_34 && dev->session_key_valid) return dev->session_key;
    return real_key(dev);
}

static void version_header(const tuya_device_t *dev, uint8_t out[TUYA_VERSION_HEADER_LEN]) {
    out[0] = '3';
    out[1] = '.';
    out[2] = (uint8_t)('0' + (dev->version % 10));
    memset(out + 3, 0, TUYA_VERSION_HEADER_LEN - 3U);
}

static WiFiClient *client_for(tuya_device_t *dev) {
    if (!dev->client) {
        dev->client = new WiFiClient();
    }
    return reinterpret_cast<WiFiClient *>(dev->client);
}

static void stop_client(tuya_device_t *dev, bool destroy) {
    WiFiClient *client = reinterpret_cast<WiFiClient *>(dev->client);
    if (client) {
        client->stop();
        if (destroy) {
            delete client;
            dev->client = nullptr;
        }
    }
    dev->session_key_valid = false;
}

static bool valid_key_for_version(const tuya_device_t *dev) {
    if (dev->version == TUYA_PROTO_31) return true;
    return strlen(dev->local_key) == TUYA_LOCAL_KEY_LEN;
}

static tuya_err_t copy_result(char *out, size_t out_len, const uint8_t *data, size_t len) {
    if (!out || out_len == 0) return TUYA_OK;
    if (len + 1U > out_len) return TUYA_ERR_BUFFER_TOO_SMALL;
    memcpy(out, data, len);
    out[len] = '\0';
    return TUYA_OK;
}

static void strip_source_header(uint8_t **payload, size_t *len) {
    if (!payload || !*payload || !len) return;
    uint8_t *p = *payload;
    if (*len > 27U && p[0] == '3' && p[1] == '.' &&
        p[3] != 0 && p[27] == '{') {
        *payload = p + 27U;
        *len -= 27U;
    }
}

static void strip_clear_source_header_before_cipher(uint8_t **payload, size_t *len) {
    if (!payload || !*payload || !len) return;
    uint8_t *p = *payload;
    if (*len > 27U && p[0] == '3' && p[1] == '.' && p[3] != 0 &&
        (((*len - 27U) % 16U) == 0U)) {
        *payload = p + 27U;
        *len -= 27U;
    }
}

static bool contains_bytes(const uint8_t *haystack, size_t haystack_len,
                           const char *needle, size_t needle_len) {
    if (!haystack || !needle || needle_len == 0U || haystack_len < needle_len) return false;
    for (size_t i = 0; i <= haystack_len - needle_len; ++i) {
        if (memcmp(haystack + i, needle, needle_len) == 0) return true;
    }
    return false;
}

static void strip_version_header(const tuya_device_t *dev, uint8_t **payload, size_t *len) {
    if (!payload || !*payload || !len || *len < 3U) return;
    uint8_t hdr[TUYA_VERSION_HEADER_LEN];
    version_header(dev, hdr);
    if (*len >= TUYA_VERSION_HEADER_LEN && memcmp(*payload, hdr, TUYA_VERSION_HEADER_LEN) == 0) {
        *payload += TUYA_VERSION_HEADER_LEN;
        *len -= TUYA_VERSION_HEADER_LEN;
    }
}

static tuya_err_t md5_slice_payload(const uint8_t *base64_payload,
                                    size_t base64_len,
                                    const uint8_t key[16],
                                    uint8_t out[16]) {
    uint8_t tmp[256];
    const uint8_t prefix[] = "data=";
    const uint8_t mid[] = "||lpv=3.1||";
    size_t need = (sizeof(prefix) - 1U) + base64_len + (sizeof(mid) - 1U) + 16U;
    if (need > sizeof(tmp)) return TUYA_ERR_BUFFER_TOO_SMALL;
    size_t off = 0;
    memcpy(tmp + off, prefix, sizeof(prefix) - 1U);
    off += sizeof(prefix) - 1U;
    memcpy(tmp + off, base64_payload, base64_len);
    off += base64_len;
    memcpy(tmp + off, mid, sizeof(mid) - 1U);
    off += sizeof(mid) - 1U;
    memcpy(tmp + off, key, 16U);
    off += 16U;

    uint8_t digest[16];
    tuya_err_t err = tuya_crypto_md5(tmp, off, digest);
    if (err != TUYA_OK) return err;

    static const char hex[] = "0123456789abcdef";
    char full[33];
    for (size_t i = 0; i < 16U; ++i) {
        full[i * 2U] = hex[digest[i] >> 4];
        full[i * 2U + 1U] = hex[digest[i] & 0x0F];
    }
    memcpy(out, full + 8, 16U);
    return TUYA_OK;
}

static tuya_err_t encode_message(tuya_device_t *dev,
                                 tuya_cmd_t cmd,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 uint8_t *out,
                                 size_t *out_len) {
    HeapBuffer plain_buf(TUYA_MAX_PAYLOAD_LENGTH);
    HeapBuffer encrypted_buf(TUYA_MAX_PAYLOAD_LENGTH + 32U);
    if (!plain_buf.ok() || !encrypted_buf.ok()) return TUYA_ERR_NOMEM;
    uint8_t *plain = plain_buf.data();
    uint8_t *encrypted = encrypted_buf.data();
    const uint8_t *wire_payload = payload;
    size_t wire_len = payload_len;
    const uint8_t *hmac_key = nullptr;
    tuya_err_t err;

    if (payload_len > TUYA_MAX_PAYLOAD_LENGTH) return TUYA_ERR_BUFFER_TOO_SMALL;

    if (dev->version >= TUYA_PROTO_34) {
        const uint8_t *key = active_key(dev);
        if (!tuya_cmd_skips_protocol_header(cmd)) {
            if (payload_len + TUYA_VERSION_HEADER_LEN > plain_buf.size()) return TUYA_ERR_BUFFER_TOO_SMALL;
            version_header(dev, plain);
            memcpy(plain + TUYA_VERSION_HEADER_LEN, payload, payload_len);
            wire_payload = plain;
            wire_len = payload_len + TUYA_VERSION_HEADER_LEN;
        }

        if (dev->version == TUYA_PROTO_35) {
            err = tuya_protocol_pack_6699(dev->seqno++, cmd, wire_payload, wire_len,
                                          key, nullptr, out, out_len);
            return err;
        }

        size_t enc_len = encrypted_buf.size();
        err = tuya_crypto_aes_ecb_encrypt_pkcs7(key, wire_payload, wire_len, encrypted, &enc_len);
        if (err != TUYA_OK) return err;
        wire_payload = encrypted;
        wire_len = enc_len;
        hmac_key = key;
    } else if (dev->version >= TUYA_PROTO_32) {
        size_t enc_len = encrypted_buf.size();
        err = tuya_crypto_aes_ecb_encrypt_pkcs7(real_key(dev), payload, payload_len, encrypted, &enc_len);
        if (err != TUYA_OK) return err;
        if (!tuya_cmd_skips_protocol_header(cmd)) {
            if (enc_len + TUYA_VERSION_HEADER_LEN > plain_buf.size()) return TUYA_ERR_BUFFER_TOO_SMALL;
            version_header(dev, plain);
            memcpy(plain + TUYA_VERSION_HEADER_LEN, encrypted, enc_len);
            wire_payload = plain;
            wire_len = enc_len + TUYA_VERSION_HEADER_LEN;
        } else {
            wire_payload = encrypted;
            wire_len = enc_len;
        }
    } else if (cmd == TUYA_CMD_CONTROL) {
        size_t enc_len = encrypted_buf.size();
        err = tuya_crypto_aes_ecb_encrypt_pkcs7(real_key(dev), payload, payload_len, encrypted, &enc_len);
        if (err != TUYA_OK) return err;

        uint8_t b64[512];
        size_t b64_len = sizeof(b64);
        err = tuya_crypto_base64_encode(encrypted, enc_len, b64, &b64_len);
        if (err != TUYA_OK) return err;

        uint8_t md5slice[16];
        err = md5_slice_payload(b64, b64_len, real_key(dev), md5slice);
        if (err != TUYA_OK) return err;
        if (3U + 16U + b64_len > plain_buf.size()) return TUYA_ERR_BUFFER_TOO_SMALL;
        memcpy(plain, VERSION_31, 3U);
        memcpy(plain + 3U, md5slice, 16U);
        memcpy(plain + 19U, b64, b64_len);
        wire_payload = plain;
        wire_len = 19U + b64_len;
    }

    return tuya_protocol_pack_55aa(dev->seqno++, cmd, wire_payload, wire_len, hmac_key, out, out_len);
}

static tuya_err_t read_exact(WiFiClient *client, uint8_t *out, size_t len, uint32_t timeout_ms) {
    uint32_t start = millis();
    size_t got = 0;
    while (got < len) {
        if (client->available()) {
            int c = client->read();
            if (c >= 0) {
                out[got++] = (uint8_t)c;
                continue;
            }
        }
        if (!client->connected() && !client->available()) return TUYA_ERR_OFFLINE;
        if (millis() - start > timeout_ms) return TUYA_ERR_TIMEOUT;
        delay(1);
    }
    return TUYA_OK;
}

static tuya_err_t read_frame(tuya_device_t *dev, uint8_t *frame, size_t *frame_len) {
    WiFiClient *client = reinterpret_cast<WiFiClient *>(dev->client);
    if (!client || !frame || !frame_len || *frame_len < 18U) return TUYA_ERR_INVAL;

    uint8_t prefix[4] = {0, 0, 0, 0};
    uint32_t start = millis();
    while (true) {
        if (client->available()) {
            int c = client->read();
            if (c >= 0) {
                prefix[0] = prefix[1];
                prefix[1] = prefix[2];
                prefix[2] = prefix[3];
                prefix[3] = (uint8_t)c;
                uint32_t p = tuya_read_be32(prefix);
                if (p == TUYA_PREFIX_55AA || p == TUYA_PREFIX_6699) break;
            }
        }
        if (!client->connected() && !client->available()) return TUYA_ERR_OFFLINE;
        if (millis() - start > dev->timeout_ms) return TUYA_ERR_TIMEOUT;
        delay(1);
    }

    memcpy(frame, prefix, 4U);
    uint32_t p = tuya_read_be32(prefix);
    size_t header_len = p == TUYA_PREFIX_6699 ? 18U : 16U;
    tuya_err_t err = read_exact(client, frame + 4U, header_len - 4U, dev->timeout_ms);
    if (err != TUYA_OK) return err;

    tuya_header_t header;
    err = tuya_protocol_parse_header(frame, header_len, &header);
    if (err != TUYA_OK) return err;
    if (header.total_length > *frame_len) return TUYA_ERR_BUFFER_TOO_SMALL;

    err = read_exact(client, frame + header_len, header.total_length - header_len, dev->timeout_ms);
    if (err != TUYA_OK) return err;
    *frame_len = header.total_length;
    return TUYA_OK;
}

static tuya_err_t decode_payload(tuya_device_t *dev,
                                 tuya_cmd_t cmd,
                                 const uint8_t *payload,
                                 size_t payload_len,
                                 char *out_json,
                                 size_t out_len) {
    (void)cmd;
    HeapBuffer work_buf(TUYA_MAX_PAYLOAD_LENGTH + 1U);
    if (!work_buf.ok()) return TUYA_ERR_NOMEM;
    uint8_t *work = work_buf.data();
    if (payload_len > TUYA_MAX_PAYLOAD_LENGTH) return TUYA_ERR_BUFFER_TOO_SMALL;
    memcpy(work, payload, payload_len);
    size_t len = payload_len;
    uint8_t *p = work;
    tuya_err_t err;

    if (dev->version == TUYA_PROTO_34) {
        size_t dec_len = work_buf.size() - 1U;
        err = tuya_crypto_aes_ecb_decrypt_pkcs7(active_key(dev), p, len, work, &dec_len);
        if (err != TUYA_OK) return err;
        p = work;
        len = dec_len;
    }

    if (len >= 3U && memcmp(p, VERSION_31, 3U) == 0) {
        if (len <= 19U) return TUYA_ERR_PAYLOAD;
        HeapBuffer decoded_buf(TUYA_MAX_PAYLOAD_LENGTH);
        if (!decoded_buf.ok()) return TUYA_ERR_NOMEM;
        uint8_t *decoded = decoded_buf.data();
        size_t decoded_len = decoded_buf.size();
        err = tuya_crypto_base64_decode(p + 19U, len - 19U, decoded, &decoded_len);
        if (err != TUYA_OK) return err;
        len = work_buf.size() - 1U;
        err = tuya_crypto_aes_ecb_decrypt_pkcs7(real_key(dev), decoded, decoded_len, work, &len);
        if (err != TUYA_OK) return err;
        p = work;
    } else if (dev->version >= TUYA_PROTO_32) {
        strip_version_header(dev, &p, &len);

        if (dev->version < TUYA_PROTO_34) {
            strip_clear_source_header_before_cipher(&p, &len);
            HeapBuffer decrypted_buf(TUYA_MAX_PAYLOAD_LENGTH + 1U);
            if (!decrypted_buf.ok()) return TUYA_ERR_NOMEM;
            uint8_t *decrypted = decrypted_buf.data();
            size_t dec_len = decrypted_buf.size() - 1U;
            err = tuya_crypto_aes_ecb_decrypt_pkcs7(real_key(dev), p, len, decrypted, &dec_len);
            if (err != TUYA_OK) return err;
            memcpy(work, decrypted, dec_len);
            p = work;
            len = dec_len;
        }

        strip_version_header(dev, &p, &len);
        strip_source_header(&p, &len);

        if (!dev->disabledetect && (dev->version == TUYA_PROTO_33 || dev->version == TUYA_PROTO_34)) {
            if (contains_bytes(p, len, "data unvalid", 12U)) {
                dev->is_device22 = true;
                return TUYA_ERR_DEVTYPE;
            }
        }
    } else if (len == 0 || p[0] != '{') {
        return TUYA_ERR_PAYLOAD;
    }

    strip_source_header(&p, &len);
    if (len == 0 || p[0] != '{') return TUYA_ERR_JSON;
    p[len] = '\0';
    err = tuya_json_validate(reinterpret_cast<const char *>(p));
    if (err != TUYA_OK) return err;
    return copy_result(out_json, out_len, p, len);
}

static tuya_err_t send_encoded(tuya_device_t *dev,
                               tuya_cmd_t cmd,
                               const uint8_t *payload,
                               size_t payload_len,
                               bool payload_required,
                               tuya_message_t *msg,
                               uint8_t *decoded_payload,
                               size_t *decoded_len) {
    HeapBuffer frame_buf(TUYA_MAX_FRAME_LENGTH);
    if (!frame_buf.ok()) return TUYA_ERR_NOMEM;
    uint8_t *frame = frame_buf.data();
    size_t frame_len = frame_buf.size();
    tuya_err_t err = encode_message(dev, cmd, payload, payload_len, frame, &frame_len);
    if (err != TUYA_OK) return err;

    WiFiClient *client = reinterpret_cast<WiFiClient *>(dev->client);
    if (!client) return TUYA_ERR_NOT_CONNECTED;
    size_t written = client->write(frame, frame_len);
    if (written != frame_len) return TUYA_ERR_OFFLINE;
    client->flush();

    for (int attempt = 0; attempt < 2; ++attempt) {
        size_t rx_len = frame_buf.size();
        err = read_frame(dev, frame, &rx_len);
        if (err != TUYA_OK) return err;

        size_t payload_cap = *decoded_len;
        const uint8_t *key = nullptr;
        if (dev->version >= TUYA_PROTO_34) key = active_key(dev);
        err = tuya_protocol_unpack(frame, rx_len, key, decoded_payload, &payload_cap, msg);
        if (err != TUYA_OK) return err;
        *decoded_len = payload_cap;
        if (payload_cap > 0U) return TUYA_OK;
        if (!payload_required) return TUYA_OK;
    }
    return TUYA_OK;
}

static tuya_err_t ensure_connected(tuya_device_t *dev);

static tuya_err_t negotiate_session(tuya_device_t *dev) {
    uint8_t local_nonce[16];
    uint8_t remote_nonce[16];
    uint8_t finish_payload[32];
    HeapBuffer payload_heap(TUYA_MAX_PAYLOAD_LENGTH);
    if (!payload_heap.ok()) return TUYA_ERR_NOMEM;
    uint8_t *payload_buf = payload_heap.data();
    tuya_message_t msg;

    tuya_session_generate_nonce(local_nonce);

    size_t decoded_len = payload_heap.size();
    tuya_err_t err = send_encoded(dev, TUYA_CMD_SESS_KEY_START, local_nonce, sizeof(local_nonce),
                                  true,
                                  &msg, payload_buf, &decoded_len);
    if (err != TUYA_OK) return err;
    if (msg.cmd != TUYA_CMD_SESS_KEY_RESP) return TUYA_ERR_KEY_OR_VER;

    err = tuya_session_build_finish(real_key(dev), local_nonce, payload_buf, decoded_len,
                                    dev->version, remote_nonce, finish_payload);
    if (err != TUYA_OK) return err;

    decoded_len = payload_heap.size();
    err = send_encoded(dev, TUYA_CMD_SESS_KEY_FINISH, finish_payload, sizeof(finish_payload),
                       false,
                       &msg, payload_buf, &decoded_len);
    if (err != TUYA_OK && err != TUYA_ERR_TIMEOUT) return err;

    err = tuya_session_finalize(real_key(dev), local_nonce, remote_nonce, dev->version, dev->session_key);
    if (err == TUYA_OK) dev->session_key_valid = true;
    return err;
}

static tuya_err_t ensure_connected(tuya_device_t *dev) {
    if (!dev) return TUYA_ERR_INVAL;
    if (!valid_key_for_version(dev)) return TUYA_ERR_KEY_OR_VER;
    WiFiClient *client = client_for(dev);
    if (!client) return TUYA_ERR_NOMEM;

    if (client->connected()) {
        if (dev->version < TUYA_PROTO_34 || dev->session_key_valid) return TUYA_OK;
    }

    stop_client(dev, false);
    client->setTimeout(dev->timeout_ms);
    for (uint8_t attempt = 0; attempt < dev->retry_limit; ++attempt) {
        if (client->connect(dev->ip, dev->port, dev->timeout_ms)) {
            if (dev->version >= TUYA_PROTO_34) {
                tuya_err_t err = negotiate_session(dev);
                if (err == TUYA_OK) return TUYA_OK;
                stop_client(dev, false);
                return err;
            }
            return TUYA_OK;
        }
        delay(100);
    }
    return TUYA_ERR_CONNECT;
}

static tuya_err_t command_roundtrip(tuya_device_t *dev,
                                    tuya_cmd_t cmd,
                                    const char *json,
                                    bool payload_required,
                                    char *out_json,
                                    size_t out_len) {
    if (!dev || !json) return TUYA_ERR_INVAL;
    tuya_err_t err = ensure_connected(dev);
    if (err != TUYA_OK) return err;

    HeapBuffer payload_heap(TUYA_MAX_PAYLOAD_LENGTH);
    if (!payload_heap.ok()) return TUYA_ERR_NOMEM;
    uint8_t *payload_buf = payload_heap.data();
    size_t payload_len = payload_heap.size();
    tuya_message_t msg;
    err = send_encoded(dev, cmd, reinterpret_cast<const uint8_t *>(json), strlen(json),
                       payload_required,
                       &msg, payload_buf, &payload_len);
    if (err == TUYA_OK && payload_len > 0U) {
        err = decode_payload(dev, msg.cmd, payload_buf, payload_len, out_json, out_len);
    }

    if (!dev->persistent) stop_client(dev, false);
    if (dev->callback) {
        if (err == TUYA_OK && out_json) dev->callback(TUYA_EVENT_STATUS_RECEIVED, out_json, dev->callback_user);
        else if (err != TUYA_OK) dev->callback(TUYA_EVENT_ERROR, nullptr, dev->callback_user);
    }
    return err;
}

tuya_err_t tuya_device_init(tuya_device_t *dev,
                            const char *device_id,
                            const char *ip,
                            const char *local_key) {
    if (!dev || !device_id || !ip) return TUYA_ERR_INVAL;
    memset(dev, 0, sizeof(*dev));
    strncpy(dev->id, device_id, sizeof(dev->id) - 1U);
    strncpy(dev->ip, ip, sizeof(dev->ip) - 1U);
    if (local_key) strncpy(dev->local_key, local_key, sizeof(dev->local_key) - 1U);
    dev->version = TUYA_PROTO_31;
    dev->seqno = 1;
    dev->timeout_ms = TUYA_TCP_TIMEOUT_MS;
    dev->retry_limit = TUYA_CONNECT_RETRIES;
    dev->port = TUYA_TCP_PORT;
    return TUYA_OK;
}

void tuya_device_set_version(tuya_device_t *dev, tuya_protocol_version_t version) {
    if (!dev) return;
    dev->version = version;
    dev->session_key_valid = false;
    if (version == TUYA_PROTO_32) dev->is_device22 = true;
}

void tuya_device_set_persistent(tuya_device_t *dev, bool persistent) {
    if (!dev) return;
    dev->persistent = persistent;
    if (!persistent) stop_client(dev, false);
}

void tuya_device_set_callback(tuya_device_t *dev, tuya_event_cb_t callback, void *user) {
    if (!dev) return;
    dev->callback = callback;
    dev->callback_user = user;
}

void tuya_device_close(tuya_device_t *dev) {
    if (!dev) return;
    stop_client(dev, true);
}

tuya_err_t tuya_status(tuya_device_t *dev, char *out_json, size_t out_len) {
    if (!dev) return TUYA_ERR_INVAL;
    char json[256];
    tuya_cmd_t cmd;
    tuya_err_t err = tuya_json_build_status(dev, &cmd, json, sizeof(json));
    if (err != TUYA_OK) return err;
    err = command_roundtrip(dev, cmd, json, true, out_json, out_len);
    if (err == TUYA_ERR_DEVTYPE) {
        err = tuya_json_build_status(dev, &cmd, json, sizeof(json));
        if (err != TUYA_OK) return err;
        err = command_roundtrip(dev, cmd, json, true, out_json, out_len);
    }
    return err;
}

tuya_err_t tuya_set_value_json(tuya_device_t *dev,
                               uint8_t dp,
                               const char *value_json,
                               char *out_json,
                               size_t out_len) {
    if (!dev || !value_json) return TUYA_ERR_INVAL;
    if (dev->version == TUYA_PROTO_31 && strlen(dev->local_key) != TUYA_LOCAL_KEY_LEN) {
        return TUYA_ERR_KEY_OR_VER;
    }
    char json[256];
    tuya_cmd_t cmd;
    tuya_err_t err = tuya_json_build_set(dev, dp, value_json, &cmd, json, sizeof(json));
    if (err != TUYA_OK) return err;
    return command_roundtrip(dev, cmd, json, false, out_json, out_len);
}

tuya_err_t tuya_set_bool(tuya_device_t *dev, uint8_t dp, bool value) {
    return tuya_set_value_json(dev, dp, value ? "true" : "false", nullptr, 0);
}

tuya_err_t tuya_set_int(tuya_device_t *dev, uint8_t dp, int value) {
    char literal[24];
    snprintf(literal, sizeof(literal), "%d", value);
    return tuya_set_value_json(dev, dp, literal, nullptr, 0);
}

tuya_err_t tuya_set_string(tuya_device_t *dev, uint8_t dp, const char *value) {
    if (!value) return TUYA_ERR_INVAL;
    char literal[160];
    size_t off = 0;
    literal[off++] = '"';
    for (const char *p = value; *p && off + 3U < sizeof(literal); ++p) {
        if (*p == '"' || *p == '\\') literal[off++] = '\\';
        literal[off++] = *p;
    }
    if (off + 1U >= sizeof(literal)) return TUYA_ERR_BUFFER_TOO_SMALL;
    literal[off++] = '"';
    literal[off] = '\0';
    return tuya_set_value_json(dev, dp, literal, nullptr, 0);
}

tuya_err_t tuya_heartbeat(tuya_device_t *dev) {
    if (!dev) return TUYA_ERR_INVAL;
    char json[96];
    tuya_cmd_t cmd;
    tuya_err_t err = tuya_json_build_heartbeat(dev, &cmd, json, sizeof(json));
    if (err != TUYA_OK) return err;
    return command_roundtrip(dev, cmd, json, false, nullptr, 0);
}
