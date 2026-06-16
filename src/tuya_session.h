#ifndef TUYA_SESSION_H
#define TUYA_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "tuya_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void tuya_session_generate_nonce(uint8_t nonce[16]);
tuya_err_t tuya_session_build_finish(const uint8_t local_key[16],
                                     const uint8_t local_nonce[16],
                                     const uint8_t *response_payload,
                                     size_t response_len,
                                     tuya_protocol_version_t version,
                                     uint8_t remote_nonce[16],
                                     uint8_t finish_payload[32]);
tuya_err_t tuya_session_finalize(const uint8_t local_key[16],
                                 const uint8_t local_nonce[16],
                                 const uint8_t remote_nonce[16],
                                 tuya_protocol_version_t version,
                                 uint8_t session_key[16]);

#ifdef __cplusplus
}
#endif

#endif
