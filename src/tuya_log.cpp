#include "tuya_log.h"

#include <stdarg.h>

static tuya_log_level_t g_level = TUYA_LOG_ERROR;
static Stream *g_out = &Serial;

void tuya_log_set_level(tuya_log_level_t level) {
    g_level = level;
}

void tuya_log_set_output(Stream *out) {
    g_out = out ? out : &Serial;
}

void tuya_log(tuya_log_level_t level, const char *fmt, ...) {
    if (level > g_level || level == TUYA_LOG_NONE || !g_out || !fmt) return;
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_out->println(buf);
}
