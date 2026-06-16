#ifndef TINYTUYA_H
#define TINYTUYA_H

#include "tuya_const.h"
#include "tuya_crypto.h"
#include "tuya_device.h"
#include "tuya_fsm.h"
#include "tuya_json.h"
#include "tuya_log.h"
#include "tuya_protocol.h"
#include "tuya_scanner.h"
#include "tuya_session.h"
#include "tuya_types.h"

#ifdef __cplusplus
#include <Arduino.h>

/**
 * Arduino-style wrapper around tuya_device_t.
 */
class TinyTuya {
public:
    TinyTuya(const char *deviceId, const char *ip, const char *localKey);
    ~TinyTuya();

    void setVersion(float version);
    void setPersistent(bool persistent);
    void setCallback(tuya_event_cb_t callback, void *user = nullptr);
    void close();

    tuya_err_t status(String &out);
    tuya_err_t setBool(uint8_t dp, bool value);
    tuya_err_t setInt(uint8_t dp, int value);
    tuya_err_t setString(uint8_t dp, const char *value);
    tuya_err_t setValue(uint8_t dp, const char *jsonLiteral, String *out = nullptr);
    tuya_err_t heartbeat();

    tuya_device_t *raw();

private:
    tuya_device_t dev_;
};

/**
 * Arduino scanner facade.
 */
class TuyaScanner {
public:
    static tuya_err_t scan(uint16_t seconds, tuya_device_found_cb_t callback, void *user = nullptr);
};

/**
 * Loop-friendly facade over tuya_fsm_t.
 */
class TinyTuyaFsm {
public:
    TinyTuyaFsm();
    ~TinyTuyaFsm();
    tuya_err_t begin(const char *deviceId,
                     const char *ip,
                     const char *localKey,
                     float version,
                     tuya_event_cb_t callback,
                     void *user = nullptr);
    void requestStatus();
    void requestSet(uint8_t dp, const char *jsonLiteral);
    void loop();
    void disconnect();
    tuya_fsm_state_t state() const;

private:
    tuya_device_t dev_;
    tuya_fsm_t fsm_;
};

#endif

#endif
