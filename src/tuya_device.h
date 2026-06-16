#ifndef TUYA_DEVICE_H
#define TUYA_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tuya_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize a Tuya device descriptor.
 *
 * @param dev Device descriptor owned by the caller.
 * @param device_id Tuya device ID / gwId.
 * @param ip IPv4 address as dotted decimal text.
 * @param local_key 16-byte local key. May be empty for v3.1 status-only usage.
 * @return TUYA_OK on success.
 */
tuya_err_t tuya_device_init(tuya_device_t *dev,
                            const char *device_id,
                            const char *ip,
                            const char *local_key);

/**
 * Set the Tuya LAN protocol revision used for subsequent requests.
 */
void tuya_device_set_version(tuya_device_t *dev, tuya_protocol_version_t version);

/**
 * Keep the TCP socket open between commands when enabled.
 */
void tuya_device_set_persistent(tuya_device_t *dev, bool persistent);

/**
 * Register an optional event callback for status, discovery, and error events.
 */
void tuya_device_set_callback(tuya_device_t *dev, tuya_event_cb_t callback, void *user);

/**
 * Close the TCP socket and clear any negotiated session key.
 */
void tuya_device_close(tuya_device_t *dev);

/**
 * Query device DPS state.
 *
 * @param out_json Buffer receiving the decoded JSON response.
 * @param out_len Size of out_json in bytes.
 */
tuya_err_t tuya_status(tuya_device_t *dev, char *out_json, size_t out_len);

/**
 * Set a raw JSON value for a datapoint. value_json must be a compact JSON literal.
 */
tuya_err_t tuya_set_value_json(tuya_device_t *dev,
                               uint8_t dp,
                               const char *value_json,
                               char *out_json,
                               size_t out_len);

/**
 * Set a boolean datapoint.
 */
tuya_err_t tuya_set_bool(tuya_device_t *dev, uint8_t dp, bool value);

/**
 * Set an integer datapoint.
 */
tuya_err_t tuya_set_int(tuya_device_t *dev, uint8_t dp, int value);

/**
 * Set a string datapoint.
 */
tuya_err_t tuya_set_string(tuya_device_t *dev, uint8_t dp, const char *value);

/**
 * Send HEART_BEAT to keep a persistent connection alive.
 */
tuya_err_t tuya_heartbeat(tuya_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif
