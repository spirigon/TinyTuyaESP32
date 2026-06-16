#include "tuya_session.h"

#include <string.h>

#include "tuya_crypto.h"

void tuya_session_generate_nonce(uint8_t nonce[16]) {
    tuya_crypto_random_bytes(nonce, 16U);
}

tuya_err_t tuya_session_build_finish(const uint8_t local_key[16],
                                     const uint8_t local_nonce[16],
                                     const uint8_t *response_payload,
                                     size_t response_len,
                                     tuya_protocol_version_t version,
                                     uint8_t remote_nonce[16],
                                     uint8_t finish_payload[32]) {
    if (!local_key || !local_nonce || !response_payload || !remote_nonce || !finish_payload) {
        return TUYA_ERR_INVAL;
    }

    uint8_t plain[80];
    size_t plain_len = sizeof(plain);
    const uint8_t *payload = response_payload;
    size_t payload_len = response_len;

    if (version == TUYA_PROTO_34) {
        tuya_err_t err = tuya_crypto_aes_ecb_decrypt_pkcs7(local_key, response_payload,
                                                           response_len, plain, &plain_len);
        if (err != TUYA_OK) return err;
        payload = plain;
        payload_len = plain_len;
    }

    if (payload_len < 48U) return TUYA_ERR_PAYLOAD;

    uint8_t expected[32];
    tuya_err_t err = tuya_crypto_hmac_sha256(local_key, 16, local_nonce, 16, expected);
    if (err != TUYA_OK) return err;
    if (memcmp(expected, payload + 16, 32U) != 0) return TUYA_ERR_KEY_OR_VER;

    memcpy(remote_nonce, payload, 16U);
    return tuya_crypto_hmac_sha256(local_key, 16, remote_nonce, 16, finish_payload);
}

tuya_err_t tuya_session_finalize(const uint8_t local_key[16],
                                 const uint8_t local_nonce[16],
                                 const uint8_t remote_nonce[16],
                                 tuya_protocol_version_t version,
                                 uint8_t session_key[16]) {
    if (!local_key || !local_nonce || !remote_nonce || !session_key) return TUYA_ERR_INVAL;

    uint8_t xored[16];
    for (size_t i = 0; i < sizeof(xored); ++i) {
        xored[i] = local_nonce[i] ^ remote_nonce[i];
    }

    if (version == TUYA_PROTO_34) {
        size_t out_len = 16U;
        return tuya_crypto_aes_ecb_encrypt_raw(local_key, xored, 16U, session_key, &out_len);
    }

    if (version == TUYA_PROTO_35) {
        uint8_t ciphertext[16];
        uint8_t tag[16];
        return tuya_crypto_aes_gcm_encrypt(local_key, local_nonce, NULL, 0,
                                           xored, 16U, ciphertext, tag) == TUYA_OK
            ? (memcpy(session_key, ciphertext, 16U), TUYA_OK)
            : TUYA_ERR_PAYLOAD;
    }

    return TUYA_ERR_UNSUPPORTED;
}
