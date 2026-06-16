#ifndef TUYA_TYPES_H
#define TUYA_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tuya_const.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TUYA_OK                 = 0,
    TUYA_ERR_OFFLINE        = -1,
    TUYA_ERR_CONNECT        = -2,
    TUYA_ERR_KEY_OR_VER     = -3,
    TUYA_ERR_PAYLOAD        = -4,
    TUYA_ERR_JSON           = -5,
    TUYA_ERR_DEVTYPE        = -6,
    TUYA_ERR_TIMEOUT        = -7,
    TUYA_ERR_CRC            = -8,
    TUYA_ERR_GCM_TAG        = -9,
    TUYA_ERR_NOMEM          = -10,
    TUYA_ERR_INVAL          = -11,
    TUYA_ERR_NOT_CONNECTED  = -12,
    TUYA_ERR_UNSUPPORTED    = -13,
    TUYA_ERR_BUFFER_TOO_SMALL = -14
} tuya_err_t;

typedef enum {
    TUYA_PROTO_31 = 31,
    TUYA_PROTO_32 = 32,
    TUYA_PROTO_33 = 33,
    TUYA_PROTO_34 = 34,
    TUYA_PROTO_35 = 35
} tuya_protocol_version_t;

typedef enum {
    TUYA_EVENT_DEVICE_FOUND,
    TUYA_EVENT_STATUS_RECEIVED,
    TUYA_EVENT_STATUS_CHANGED,
    TUYA_EVENT_ERROR
} tuya_event_t;

typedef struct {
    char ip[16];
    char gw_id[32];
    char product_key[48];
    char version[8];
    char raw_json[TUYA_MAX_JSON_LENGTH];
} tuya_device_info_t;

typedef void (*tuya_event_cb_t)(tuya_event_t event, const char *payload, void *user);
typedef void (*tuya_device_found_cb_t)(const tuya_device_info_t *info, void *user);

typedef struct {
    char id[TUYA_DEVICE_ID_MAX];
    char ip[16];
    char local_key[TUYA_LOCAL_KEY_LEN + 1];
    uint8_t session_key[TUYA_LOCAL_KEY_LEN];
    tuya_protocol_version_t version;
    bool session_key_valid;
    bool persistent;
    bool is_device22;
    bool disabledetect;
    uint32_t seqno;
    uint32_t timeout_ms;
    uint8_t retry_limit;
    uint16_t port;
    void *client;
    tuya_event_cb_t callback;
    void *callback_user;
} tuya_device_t;

#ifdef __cplusplus
}
#endif

#endif
