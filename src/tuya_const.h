#ifndef TUYA_CONST_H
#define TUYA_CONST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TUYA_UDP_PORT_31          6666
#define TUYA_UDP_PORT_33          6667
#define TUYA_UDP_PORT_APP         7000
#define TUYA_TCP_PORT             6668
#define TUYA_DISCOVERY_SECONDS    18
#define TUYA_TCP_TIMEOUT_MS       3000
#define TUYA_CONNECT_RETRIES      3
#define TUYA_MAX_PAYLOAD_LENGTH   1440
#define TUYA_MAX_FRAME_LENGTH     2048
#define TUYA_MAX_JSON_LENGTH      1024
#define TUYA_DEVICE_ID_MAX        32
#define TUYA_LOCAL_KEY_LEN        16
#define TUYA_VERSION_HEADER_LEN   15

#define TUYA_PREFIX_55AA          0x000055AAUL
#define TUYA_SUFFIX_55AA          0x0000AA55UL
#define TUYA_PREFIX_6699          0x00006699UL
#define TUYA_SUFFIX_6699          0x00009966UL

typedef enum {
    TUYA_CMD_AP_CONFIG        = 0x01,
    TUYA_CMD_ACTIVE           = 0x02,
    TUYA_CMD_SESS_KEY_START   = 0x03,
    TUYA_CMD_SESS_KEY_RESP    = 0x04,
    TUYA_CMD_SESS_KEY_FINISH  = 0x05,
    TUYA_CMD_UNBIND           = 0x06,
    TUYA_CMD_CONTROL          = 0x07,
    TUYA_CMD_STATUS           = 0x08,
    TUYA_CMD_HEART_BEAT       = 0x09,
    TUYA_CMD_DP_QUERY         = 0x0A,
    TUYA_CMD_QUERY_WIFI       = 0x0B,
    TUYA_CMD_TOKEN_BIND       = 0x0C,
    TUYA_CMD_CONTROL_NEW      = 0x0D,
    TUYA_CMD_ENABLE_WIFI      = 0x0E,
    TUYA_CMD_WIFI_INFO        = 0x0F,
    TUYA_CMD_DP_QUERY_NEW     = 0x10,
    TUYA_CMD_SCENE_EXECUTE    = 0x11,
    TUYA_CMD_UPDATEDPS        = 0x12,
    TUYA_CMD_UDP_NEW          = 0x13,
    TUYA_CMD_AP_CONFIG_NEW    = 0x14,
    TUYA_CMD_BROADCAST_LPV34  = 0x23,
    TUYA_CMD_REQ_DEVINFO      = 0x25,
    TUYA_CMD_LAN_EXT_STREAM   = 0x40
} tuya_cmd_t;

static inline int tuya_cmd_skips_protocol_header(tuya_cmd_t cmd) {
    return cmd == TUYA_CMD_DP_QUERY ||
           cmd == TUYA_CMD_DP_QUERY_NEW ||
           cmd == TUYA_CMD_UPDATEDPS ||
           cmd == TUYA_CMD_HEART_BEAT ||
           cmd == TUYA_CMD_SESS_KEY_START ||
           cmd == TUYA_CMD_SESS_KEY_RESP ||
           cmd == TUYA_CMD_SESS_KEY_FINISH ||
           cmd == TUYA_CMD_LAN_EXT_STREAM;
}

#ifdef __cplusplus
}
#endif

#endif
