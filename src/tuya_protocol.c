#include "tuya_protocol.h"

#include <string.h>

#include "tuya_crypto.h"

uint32_t tuya_read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

void tuya_write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

tuya_err_t tuya_protocol_parse_header(const uint8_t *frame, size_t len, tuya_header_t *out) {
    if (!frame || !out || len < 16U) return TUYA_ERR_INVAL;
    uint32_t prefix = tuya_read_be32(frame);
    memset(out, 0, sizeof(*out));
    out->prefix = prefix;

    if (prefix == TUYA_PREFIX_55AA) {
        out->seqno = tuya_read_be32(frame + 4);
        out->cmd = (tuya_cmd_t)tuya_read_be32(frame + 8);
        out->length = tuya_read_be32(frame + 12);
        if (out->length > TUYA_MAX_PAYLOAD_LENGTH + 40U) return TUYA_ERR_PAYLOAD;
        out->total_length = out->length + 16U;
        return TUYA_OK;
    }

    if (prefix == TUYA_PREFIX_6699) {
        if (len < 18U) return TUYA_ERR_INVAL;
        (void)read_be16(frame + 4);
        out->seqno = tuya_read_be32(frame + 6);
        out->cmd = (tuya_cmd_t)tuya_read_be32(frame + 10);
        out->length = tuya_read_be32(frame + 14);
        if (out->length > TUYA_MAX_PAYLOAD_LENGTH + 28U) return TUYA_ERR_PAYLOAD;
        out->total_length = out->length + 18U + 4U;
        return TUYA_OK;
    }

    return TUYA_ERR_PAYLOAD;
}

tuya_err_t tuya_protocol_pack_55aa(uint32_t seqno, tuya_cmd_t cmd,
                                   const uint8_t *payload, size_t payload_len,
                                   const uint8_t *hmac_key,
                                   uint8_t *out, size_t *out_len) {
    if ((!payload && payload_len) || !out || !out_len) return TUYA_ERR_INVAL;
    size_t end_len = hmac_key ? 36U : 8U;
    size_t total = 16U + payload_len + end_len;
    if (*out_len < total) return TUYA_ERR_BUFFER_TOO_SMALL;

    tuya_write_be32(out, TUYA_PREFIX_55AA);
    tuya_write_be32(out + 4, seqno);
    tuya_write_be32(out + 8, (uint32_t)cmd);
    tuya_write_be32(out + 12, (uint32_t)(payload_len + end_len));
    if (payload_len) memcpy(out + 16, payload, payload_len);

    if (hmac_key) {
        uint8_t mac[32];
        tuya_err_t err = tuya_crypto_hmac_sha256(hmac_key, 16, out, 16U + payload_len, mac);
        if (err != TUYA_OK) return err;
        memcpy(out + 16U + payload_len, mac, sizeof(mac));
        tuya_write_be32(out + 16U + payload_len + 32U, TUYA_SUFFIX_55AA);
    } else {
        uint32_t crc = tuya_crc32(out, 16U + payload_len);
        tuya_write_be32(out + 16U + payload_len, crc);
        tuya_write_be32(out + 16U + payload_len + 4U, TUYA_SUFFIX_55AA);
    }

    *out_len = total;
    return TUYA_OK;
}

tuya_err_t tuya_protocol_pack_6699(uint32_t seqno, tuya_cmd_t cmd,
                                   const uint8_t *payload, size_t payload_len,
                                   const uint8_t key[16],
                                   const uint8_t *iv_or_null,
                                   uint8_t *out, size_t *out_len) {
    if ((!payload && payload_len) || !key || !out || !out_len) return TUYA_ERR_INVAL;
    size_t msg_len = 12U + payload_len + 16U;
    size_t total = 18U + msg_len + 4U;
    if (*out_len < total) return TUYA_ERR_BUFFER_TOO_SMALL;

    uint8_t iv[12];
    if (iv_or_null) memcpy(iv, iv_or_null, sizeof(iv));
    else tuya_crypto_random_bytes(iv, sizeof(iv));

    tuya_write_be32(out, TUYA_PREFIX_6699);
    write_be16(out + 4, 0);
    tuya_write_be32(out + 6, seqno);
    tuya_write_be32(out + 10, (uint32_t)cmd);
    tuya_write_be32(out + 14, (uint32_t)msg_len);
    memcpy(out + 18, iv, sizeof(iv));

    uint8_t *ciphertext = out + 30;
    uint8_t *tag = ciphertext + payload_len;
    tuya_err_t err = tuya_crypto_aes_gcm_encrypt(key, iv, out + 4, 14, payload, payload_len, ciphertext, tag);
    if (err != TUYA_OK) return err;
    tuya_write_be32(tag + 16, TUYA_SUFFIX_6699);
    *out_len = total;
    return TUYA_OK;
}

tuya_err_t tuya_protocol_unpack(const uint8_t *frame, size_t frame_len,
                                const uint8_t *auth_key,
                                uint8_t *payload_out, size_t *payload_len,
                                tuya_message_t *msg) {
    if (!frame || !payload_out || !payload_len || !msg) return TUYA_ERR_INVAL;
    tuya_header_t header;
    tuya_err_t err = tuya_protocol_parse_header(frame, frame_len, &header);
    if (err != TUYA_OK) return err;
    if (frame_len < header.total_length) return TUYA_ERR_PAYLOAD;

    memset(msg, 0, sizeof(*msg));
    msg->seqno = header.seqno;
    msg->cmd = header.cmd;
    msg->retcode = 0;
    msg->prefix = header.prefix;

    if (header.prefix == TUYA_PREFIX_55AA) {
        size_t end_len = auth_key ? 36U : 8U;
        if (header.length < end_len) return TUYA_ERR_PAYLOAD;
        size_t content_len = header.length - end_len;
        const uint8_t *content = frame + 16;
        const uint8_t *integrity = content + content_len;
        uint32_t suffix = tuya_read_be32(integrity + (auth_key ? 32U : 4U));
        if (suffix != TUYA_SUFFIX_55AA) return TUYA_ERR_PAYLOAD;

        if (auth_key) {
            uint8_t mac[32];
            err = tuya_crypto_hmac_sha256(auth_key, 16, frame, 16U + content_len, mac);
            if (err != TUYA_OK) return err;
            msg->integrity_ok = memcmp(mac, integrity, 32U) == 0;
        } else {
            uint32_t expected = tuya_crc32(frame, 16U + content_len);
            msg->integrity_ok = expected == tuya_read_be32(integrity);
        }
        if (!msg->integrity_ok) return TUYA_ERR_CRC;

        size_t ret_len = 0;
        if (content_len >= 4U && content[0] == 0 && content[1] == 0 &&
            content[2] == 0 && content[3] <= 0x10U) {
            ret_len = 4U;
            msg->retcode = (int32_t)tuya_read_be32(content);
        }
        if (*payload_len < content_len - ret_len) return TUYA_ERR_BUFFER_TOO_SMALL;
        memcpy(payload_out, content + ret_len, content_len - ret_len);
        *payload_len = content_len - ret_len;
        msg->payload = payload_out;
        msg->payload_len = *payload_len;
        return TUYA_OK;
    }

    if (header.prefix == TUYA_PREFIX_6699) {
        if (!auth_key || header.length < 28U) return TUYA_ERR_INVAL;
        const uint8_t *iv = frame + 18;
        size_t cipher_len = header.length - 12U - 16U;
        const uint8_t *ciphertext = frame + 30;
        const uint8_t *tag = ciphertext + cipher_len;
        uint32_t suffix = tuya_read_be32(tag + 16);
        if (suffix != TUYA_SUFFIX_6699) return TUYA_ERR_PAYLOAD;
        if (*payload_len < cipher_len) return TUYA_ERR_BUFFER_TOO_SMALL;

        err = tuya_crypto_aes_gcm_decrypt(auth_key, iv, frame + 4, 14,
                                          ciphertext, cipher_len, tag, payload_out);
        if (err != TUYA_OK) return err;
        memcpy(msg->iv, iv, 12U);
        msg->integrity_ok = true;

        size_t plain_len = cipher_len;
        if (plain_len >= 4U && payload_out[0] == 0 && payload_out[1] == 0 &&
            payload_out[2] == 0 && payload_out[3] <= 0x10U &&
            (plain_len == 4U || payload_out[4] == (uint8_t)'{')) {
            msg->retcode = (int32_t)tuya_read_be32(payload_out);
            memmove(payload_out, payload_out + 4, plain_len - 4U);
            plain_len -= 4U;
        }
        *payload_len = plain_len;
        msg->payload = payload_out;
        msg->payload_len = plain_len;
        return TUYA_OK;
    }

    return TUYA_ERR_PAYLOAD;
}
