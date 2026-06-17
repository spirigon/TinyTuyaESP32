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

typedef void (*tiny_tuya_multi_cb_t)(size_t index,
                                     const char *deviceId,
                                     tuya_event_t event,
                                     const char *payload,
                                     tuya_err_t err,
                                     void *user);

/**
 * Cooperative multi-device manager.
 *
 * Each slot owns an independent Tuya device/session. loop() services at most one
 * queued or due request per call, so sketches can poll many devices without
 * writing their own round-robin scheduler.
 */
class TinyTuyaMulti {
public:
    explicit TinyTuyaMulti(size_t capacity);
    ~TinyTuyaMulti();

    bool ok() const;
    size_t capacity() const;
    size_t count() const;

    int addDevice(const char *deviceId,
                  const char *ip,
                  const char *localKey,
                  float version,
                  uint32_t pollMs = 30000UL);

    void setCallback(tiny_tuya_multi_cb_t callback, void *user = nullptr);
    tuya_err_t setPollInterval(size_t index, uint32_t pollMs);
    tuya_err_t requestStatus(size_t index);
    tuya_err_t requestSet(size_t index, uint8_t dp, const char *jsonLiteral);
    tuya_err_t requestSetBool(size_t index, uint8_t dp, bool value);
    tuya_err_t requestSetInt(size_t index, uint8_t dp, int value);
    tuya_err_t requestSetString(size_t index, uint8_t dp, const char *value);
    void loop();

    void disconnect(size_t index);
    void disconnectAll();
    tuya_err_t lastError(size_t index) const;
    tuya_fsm_state_t state(size_t index) const;
    const char *deviceId(size_t index) const;
    tuya_device_t *raw(size_t index);

private:
    struct Slot;
    Slot *slotAt(size_t index) const;
    static void onFsmEvent(tuya_event_t event, const char *payload, void *user);

    Slot *slots_;
    size_t capacity_;
    size_t count_;
    size_t cursor_;
    tiny_tuya_multi_cb_t callback_;
    void *callback_user_;
};

#endif

#endif
