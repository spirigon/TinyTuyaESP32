#ifndef TUYA_LOG_H
#define TUYA_LOG_H

#ifdef __cplusplus
#include <Arduino.h>

typedef enum {
    TUYA_LOG_NONE = 0,
    TUYA_LOG_ERROR,
    TUYA_LOG_WARN,
    TUYA_LOG_INFO,
    TUYA_LOG_DEBUG,
    TUYA_LOG_TRACE
} tuya_log_level_t;

void tuya_log_set_level(tuya_log_level_t level);
void tuya_log_set_output(Stream *out);
void tuya_log(tuya_log_level_t level, const char *fmt, ...);

#define TUYA_LOG_E(fmt, ...) tuya_log(TUYA_LOG_ERROR, fmt, ##__VA_ARGS__)
#define TUYA_LOG_W(fmt, ...) tuya_log(TUYA_LOG_WARN,  fmt, ##__VA_ARGS__)
#define TUYA_LOG_I(fmt, ...) tuya_log(TUYA_LOG_INFO,  fmt, ##__VA_ARGS__)
#define TUYA_LOG_D(fmt, ...) tuya_log(TUYA_LOG_DEBUG, fmt, ##__VA_ARGS__)
#else
#define TUYA_LOG_E(...)
#define TUYA_LOG_W(...)
#define TUYA_LOG_I(...)
#define TUYA_LOG_D(...)
#endif

#endif
