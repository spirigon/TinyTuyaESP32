#include "../src/tuya_crypto.h"

tuya_err_t tuya_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                   const uint8_t *data, size_t len,
                                   uint8_t out[32]) {
    (void)key;
    (void)key_len;
    (void)data;
    (void)len;
    (void)out;
    return TUYA_ERR_UNSUPPORTED;
}

tuya_err_t tuya_crypto_aes_gcm_encrypt(const uint8_t key[16],
                                       const uint8_t iv[12],
                                       const uint8_t *aad,
                                       size_t aad_len,
                                       const uint8_t *input,
                                       size_t input_len,
                                       uint8_t *ciphertext,
                                       uint8_t tag[16]) {
    (void)key;
    (void)iv;
    (void)aad;
    (void)aad_len;
    (void)input;
    (void)input_len;
    (void)ciphertext;
    (void)tag;
    return TUYA_ERR_UNSUPPORTED;
}

tuya_err_t tuya_crypto_aes_gcm_decrypt(const uint8_t key[16],
                                       const uint8_t iv[12],
                                       const uint8_t *aad,
                                       size_t aad_len,
                                       const uint8_t *ciphertext,
                                       size_t cipher_len,
                                       const uint8_t tag[16],
                                       uint8_t *plaintext) {
    (void)key;
    (void)iv;
    (void)aad;
    (void)aad_len;
    (void)ciphertext;
    (void)cipher_len;
    (void)tag;
    (void)plaintext;
    return TUYA_ERR_UNSUPPORTED;
}

void tuya_crypto_random_bytes(uint8_t *out, size_t len) {
    if (!out) return;
    for (size_t i = 0; i < len; ++i) {
        out[i] = (uint8_t)i;
    }
}
