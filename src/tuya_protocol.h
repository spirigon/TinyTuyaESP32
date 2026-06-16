#ifndef TUYA_PROTOCOL_H
#define TUYA_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tuya_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t prefix;
    uint32_t seqno;
    tuya_cmd_t cmd;
    uint32_t length;
    uint32_t total_length;
} tuya_header_t;

typedef struct {
    uint32_t seqno;
    tuya_cmd_t cmd;
    int32_t retcode;
    uint8_t *payload;
    size_t payload_len;
    bool integrity_ok;
    uint32_t prefix;
    uint8_t iv[12];
} tuya_message_t;

tuya_err_t tuya_protocol_parse_header(const uint8_t *frame, size_t len, tuya_header_t *out);

tuya_err_t tuya_protocol_pack_55aa(uint32_t seqno, tuya_cmd_t cmd,
                                   const uint8_t *payload, size_t payload_len,
                                   const uint8_t *hmac_key,
                                   uint8_t *out, size_t *out_len);

tuya_err_t tuya_protocol_pack_6699(uint32_t seqno, tuya_cmd_t cmd,
                                   const uint8_t *payload, size_t payload_len,
                                   const uint8_t key[16],
                                   const uint8_t *iv_or_null,
                                   uint8_t *out, size_t *out_len);

tuya_err_t tuya_protocol_unpack(const uint8_t *frame, size_t frame_len,
                                const uint8_t *auth_key,
                                uint8_t *payload_out, size_t *payload_len,
                                tuya_message_t *msg);

uint32_t tuya_read_be32(const uint8_t *p);
void tuya_write_be32(uint8_t *p, uint32_t v);

#ifdef __cplusplus
}
#endif

#endif
