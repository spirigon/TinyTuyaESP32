#include "tinytuya.h"

#ifdef __cplusplus

static tuya_protocol_version_t version_from_float(float version) {
    if (version < 3.15f) return TUYA_PROTO_31;
    if (version < 3.25f) return TUYA_PROTO_32;
    if (version < 3.35f) return TUYA_PROTO_33;
    if (version < 3.45f) return TUYA_PROTO_34;
    return TUYA_PROTO_35;
}

TinyTuya::TinyTuya(const char *deviceId, const char *ip, const char *localKey) {
    tuya_device_init(&dev_, deviceId, ip, localKey);
}

TinyTuya::~TinyTuya() {
    tuya_device_close(&dev_);
}

void TinyTuya::setVersion(float version) {
    tuya_device_set_version(&dev_, version_from_float(version));
}

void TinyTuya::setPersistent(bool persistent) {
    tuya_device_set_persistent(&dev_, persistent);
}

void TinyTuya::setCallback(tuya_event_cb_t callback, void *user) {
    tuya_device_set_callback(&dev_, callback, user);
}

void TinyTuya::close() {
    tuya_device_close(&dev_);
}

tuya_err_t TinyTuya::status(String &out) {
    char buf[TUYA_MAX_JSON_LENGTH];
    tuya_err_t err = tuya_status(&dev_, buf, sizeof(buf));
    if (err == TUYA_OK) out = buf;
    return err;
}

tuya_err_t TinyTuya::setBool(uint8_t dp, bool value) {
    return tuya_set_bool(&dev_, dp, value);
}

tuya_err_t TinyTuya::setInt(uint8_t dp, int value) {
    return tuya_set_int(&dev_, dp, value);
}

tuya_err_t TinyTuya::setString(uint8_t dp, const char *value) {
    return tuya_set_string(&dev_, dp, value);
}

tuya_err_t TinyTuya::setValue(uint8_t dp, const char *jsonLiteral, String *out) {
    char buf[TUYA_MAX_JSON_LENGTH];
    tuya_err_t err = tuya_set_value_json(&dev_, dp, jsonLiteral, buf, sizeof(buf));
    if (err == TUYA_OK && out) *out = buf;
    return err;
}

tuya_err_t TinyTuya::heartbeat() {
    return tuya_heartbeat(&dev_);
}

tuya_device_t *TinyTuya::raw() {
    return &dev_;
}

tuya_err_t TuyaScanner::scan(uint16_t seconds, tuya_device_found_cb_t callback, void *user) {
    return tuya_scanner_scan(seconds, callback, user);
}

TinyTuyaFsm::TinyTuyaFsm() {
    memset(&dev_, 0, sizeof(dev_));
    memset(&fsm_, 0, sizeof(fsm_));
}

TinyTuyaFsm::~TinyTuyaFsm() {
    tuya_device_close(&dev_);
}

tuya_err_t TinyTuyaFsm::begin(const char *deviceId,
                              const char *ip,
                              const char *localKey,
                              float version,
                              tuya_event_cb_t callback,
                              void *user) {
    tuya_err_t err = tuya_device_init(&dev_, deviceId, ip, localKey);
    if (err != TUYA_OK) return err;
    tuya_device_set_version(&dev_, version_from_float(version));
    tuya_device_set_persistent(&dev_, true);
    tuya_fsm_init(&fsm_, &dev_, callback, user);
    return TUYA_OK;
}

void TinyTuyaFsm::requestStatus() {
    tuya_fsm_request_status(&fsm_);
}

void TinyTuyaFsm::requestSet(uint8_t dp, const char *jsonLiteral) {
    tuya_fsm_request_set(&fsm_, dp, jsonLiteral);
}

void TinyTuyaFsm::loop() {
    tuya_fsm_loop(&fsm_);
}

void TinyTuyaFsm::disconnect() {
    tuya_fsm_disconnect(&fsm_);
}

tuya_fsm_state_t TinyTuyaFsm::state() const {
    return fsm_.state;
}

#endif
