#include "tuya_crypto.h"

#include <string.h>

#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/md5.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_random.h>
#endif

uint32_t tuya_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

tuya_err_t tuya_crypto_md5(const uint8_t *data, size_t len, uint8_t out[16]) {
    if (!data || !out) return TUYA_ERR_INVAL;
    int rc = mbedtls_md5(data, len, out);
    return rc == 0 ? TUYA_OK : TUYA_ERR_PAYLOAD;
}

tuya_err_t tuya_crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                   const uint8_t *data, size_t len,
                                   uint8_t out[32]) {
    if (!key || !data || !out) return TUYA_ERR_INVAL;
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info) return TUYA_ERR_UNSUPPORTED;
    int rc = mbedtls_md_hmac(info, key, key_len, data, len, out);
    return rc == 0 ? TUYA_OK : TUYA_ERR_PAYLOAD;
}

tuya_err_t tuya_crypto_aes_ecb_encrypt_raw(const uint8_t key[16],
                                           const uint8_t *input,
                                           size_t input_len,
                                           uint8_t *out,
                                           size_t *out_len) {
    if (!key || !input || !out || !out_len) return TUYA_ERR_INVAL;
    if ((input_len % 16U) != 0U || *out_len < input_len) return TUYA_ERR_BUFFER_TOO_SMALL;

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int rc = mbedtls_aes_setkey_enc(&ctx, key, 128);
    for (size_t off = 0; rc == 0 && off < input_len; off += 16U) {
        rc = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, input + off, out + off);
    }
    mbedtls_aes_free(&ctx);
    if (rc != 0) return TUYA_ERR_PAYLOAD;
    *out_len = input_len;
    return TUYA_OK;
}

tuya_err_t tuya_crypto_aes_ecb_encrypt_pkcs7(const uint8_t key[16],
                                             const uint8_t *input,
                                             size_t input_len,
                                             uint8_t *out,
                                             size_t *out_len) {
    if (!key || (!input && input_len) || !out || !out_len) return TUYA_ERR_INVAL;
    size_t padded_len = input_len + (16U - (input_len % 16U));
    if (*out_len < padded_len) return TUYA_ERR_BUFFER_TOO_SMALL;

    uint8_t block[16];
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int rc = mbedtls_aes_setkey_enc(&ctx, key, 128);

    size_t off = 0;
    while (rc == 0 && off + 16U <= input_len) {
        rc = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, input + off, out + off);
        off += 16U;
    }

    if (rc == 0) {
        size_t remain = input_len - off;
        uint8_t pad = (uint8_t)(16U - remain);
        memset(block, pad, sizeof(block));
        if (remain) memcpy(block, input + off, remain);
        rc = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, block, out + off);
    }

    mbedtls_aes_free(&ctx);
    if (rc != 0) return TUYA_ERR_PAYLOAD;
    *out_len = padded_len;
    return TUYA_OK;
}

tuya_err_t tuya_crypto_aes_ecb_decrypt_pkcs7(const uint8_t key[16],
                                             const uint8_t *input,
                                             size_t input_len,
                                             uint8_t *out,
                                             size_t *out_len) {
    if (!key || !input || !out || !out_len) return TUYA_ERR_INVAL;
    if ((input_len % 16U) != 0U || *out_len < input_len || input_len == 0U) {
        return TUYA_ERR_BUFFER_TOO_SMALL;
    }

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int rc = mbedtls_aes_setkey_dec(&ctx, key, 128);
    for (size_t off = 0; rc == 0 && off < input_len; off += 16U) {
        rc = mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, input + off, out + off);
    }
    mbedtls_aes_free(&ctx);
    if (rc != 0) return TUYA_ERR_PAYLOAD;

    uint8_t pad = out[input_len - 1U];
    if (pad < 1U || pad > 16U || pad > input_len) return TUYA_ERR_PAYLOAD;
    for (uint8_t i = 0; i < pad; ++i) {
        if (out[input_len - 1U - i] != pad) return TUYA_ERR_PAYLOAD;
    }
    *out_len = input_len - pad;
    return TUYA_OK;
}

tuya_err_t tuya_crypto_aes_gcm_encrypt(const uint8_t key[16],
                                       const uint8_t iv[12],
                                       const uint8_t *aad,
                                       size_t aad_len,
                                       const uint8_t *input,
                                       size_t input_len,
                                       uint8_t *ciphertext,
                                       uint8_t tag[16]) {
    if (!key || !iv || (!input && input_len) || !ciphertext || !tag) return TUYA_ERR_INVAL;
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    int rc = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (rc == 0) {
        rc = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, input_len,
                                       iv, 12, aad, aad_len, input,
                                       ciphertext, 16, tag);
    }
    mbedtls_gcm_free(&ctx);
    return rc == 0 ? TUYA_OK : TUYA_ERR_PAYLOAD;
}

tuya_err_t tuya_crypto_aes_gcm_decrypt(const uint8_t key[16],
                                       const uint8_t iv[12],
                                       const uint8_t *aad,
                                       size_t aad_len,
                                       const uint8_t *ciphertext,
                                       size_t cipher_len,
                                       const uint8_t tag[16],
                                       uint8_t *plaintext) {
    if (!key || !iv || (!ciphertext && cipher_len) || !tag || !plaintext) return TUYA_ERR_INVAL;
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    int rc = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
    if (rc == 0) {
        rc = mbedtls_gcm_auth_decrypt(&ctx, cipher_len, iv, 12,
                                      aad, aad_len, tag, 16,
                                      ciphertext, plaintext);
    }
    mbedtls_gcm_free(&ctx);
    return rc == 0 ? TUYA_OK : TUYA_ERR_GCM_TAG;
}

tuya_err_t tuya_crypto_base64_encode(const uint8_t *input, size_t input_len,
                                     uint8_t *out, size_t *out_len) {
    if ((!input && input_len) || !out || !out_len) return TUYA_ERR_INVAL;
    size_t olen = 0;
    int rc = mbedtls_base64_encode(out, *out_len, &olen, input, input_len);
    if (rc == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) return TUYA_ERR_BUFFER_TOO_SMALL;
    if (rc != 0) return TUYA_ERR_PAYLOAD;
    *out_len = olen;
    return TUYA_OK;
}

tuya_err_t tuya_crypto_base64_decode(const uint8_t *input, size_t input_len,
                                     uint8_t *out, size_t *out_len) {
    if (!input || !out || !out_len) return TUYA_ERR_INVAL;
    size_t olen = 0;
    int rc = mbedtls_base64_decode(out, *out_len, &olen, input, input_len);
    if (rc == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) return TUYA_ERR_BUFFER_TOO_SMALL;
    if (rc != 0) return TUYA_ERR_PAYLOAD;
    *out_len = olen;
    return TUYA_OK;
}

void tuya_crypto_random_bytes(uint8_t *out, size_t len) {
    if (!out) return;
#if defined(ARDUINO_ARCH_ESP32)
    esp_fill_random(out, len);
#else
    uint32_t x = 0xA5A55A5AU;
    for (size_t i = 0; i < len; ++i) {
        x = 1664525UL * x + 1013904223UL;
        out[i] = (uint8_t)(x >> 24);
    }
#endif
}

void tuya_crypto_udp_key(uint8_t out[16]) {
    static const uint8_t seed[] = "yGAdlopoPVldABfn";
    (void)tuya_crypto_md5(seed, sizeof(seed) - 1U, out);
}
