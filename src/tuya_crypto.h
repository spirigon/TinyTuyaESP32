#ifndef TUYA_CRYPTO_H
#define TUYA_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tuya_types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t tuya_crc32(const uint8_t *data, size_t len);

tuya_err_t tuya_crypto_md5(const uint8_t *data, size_t len, uint8_t out[16]);
tuya_err_t tuya_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                   const uint8_t *data, size_t len,
                                   uint8_t out[32]);

tuya_err_t tuya_crypto_aes_ecb_encrypt_pkcs7(const uint8_t key[16],
                                             const uint8_t *input,
                                             size_t input_len,
                                             uint8_t *out,
                                             size_t *out_len);
tuya_err_t tuya_crypto_aes_ecb_encrypt_raw(const uint8_t key[16],
                                           const uint8_t *input,
                                           size_t input_len,
                                           uint8_t *out,
                                           size_t *out_len);
tuya_err_t tuya_crypto_aes_ecb_decrypt_pkcs7(const uint8_t key[16],
                                             const uint8_t *input,
                                             size_t input_len,
                                             uint8_t *out,
                                             size_t *out_len);

tuya_err_t tuya_crypto_aes_gcm_encrypt(const uint8_t key[16],
                                       const uint8_t iv[12],
                                       const uint8_t *aad,
                                       size_t aad_len,
                                       const uint8_t *input,
                                       size_t input_len,
                                       uint8_t *ciphertext,
                                       uint8_t tag[16]);
tuya_err_t tuya_crypto_aes_gcm_decrypt(const uint8_t key[16],
                                       const uint8_t iv[12],
                                       const uint8_t *aad,
                                       size_t aad_len,
                                       const uint8_t *ciphertext,
                                       size_t cipher_len,
                                       const uint8_t tag[16],
                                       uint8_t *plaintext);

tuya_err_t tuya_crypto_base64_encode(const uint8_t *input, size_t input_len,
                                     uint8_t *out, size_t *out_len);
tuya_err_t tuya_crypto_base64_decode(const uint8_t *input, size_t input_len,
                                     uint8_t *out, size_t *out_len);

void tuya_crypto_random_bytes(uint8_t *out, size_t len);
void tuya_crypto_udp_key(uint8_t out[16]);

#ifdef __cplusplus
}
#endif

#endif
