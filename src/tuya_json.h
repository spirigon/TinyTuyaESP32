#ifndef TUYA_JSON_H
#define TUYA_JSON_H

#include <stddef.h>
#include <stdint.h>

#include "tuya_types.h"

#ifdef __cplusplus
extern "C" {
#endif

tuya_err_t tuya_json_build_status(const tuya_device_t *dev,
                                  tuya_cmd_t *cmd,
                                  char *out,
                                  size_t out_len);

tuya_err_t tuya_json_build_set(const tuya_device_t *dev,
                               uint8_t dp,
                               const char *value_json,
                               tuya_cmd_t *cmd,
                               char *out,
                               size_t out_len);

tuya_err_t tuya_json_build_heartbeat(const tuya_device_t *dev,
                                     tuya_cmd_t *cmd,
                                     char *out,
                                     size_t out_len);

tuya_err_t tuya_json_validate(const char *json);
tuya_err_t tuya_json_parse_discovery(const char *json,
                                     const char *ip,
                                     tuya_device_info_t *info);

#ifdef __cplusplus
}
#endif

#endif
