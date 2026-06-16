#ifndef TUYA_SCANNER_H
#define TUYA_SCANNER_H

#include <stdint.h>

#include "tuya_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Scan for Tuya LAN broadcast packets.
 *
 * This opens UDP listeners on 6666, 6667, and 7000. During the scan it also
 * sends a v3.5 REQ_DEVINFO solicitation to UDP/7000 every six seconds.
 *
 * @param seconds Scan duration.
 * @param callback Called once per discovered gwId when possible.
 * @param user Opaque pointer passed to callback.
 * @return TUYA_OK if the scan completed.
 */
tuya_err_t tuya_scanner_scan(uint16_t seconds,
                             tuya_device_found_cb_t callback,
                             void *user);

#ifdef __cplusplus
}
#endif

#endif
