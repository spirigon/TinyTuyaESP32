#include "tinytuya.h"

#ifdef __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

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
    char *buf = static_cast<char *>(malloc(TUYA_MAX_JSON_LENGTH));
    if (!buf) return TUYA_ERR_NOMEM;
    tuya_err_t err = tuya_status(&dev_, buf, TUYA_MAX_JSON_LENGTH);
    if (err == TUYA_OK) out = buf;
    free(buf);
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
    char *buf = static_cast<char *>(malloc(TUYA_MAX_JSON_LENGTH));
    if (!buf) return TUYA_ERR_NOMEM;
    tuya_err_t err = tuya_set_value_json(&dev_, dp, jsonLiteral, buf, TUYA_MAX_JSON_LENGTH);
    if (err == TUYA_OK && out) *out = buf;
    free(buf);
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

struct TinyTuyaAsync::AsyncRequest {
    enum Type : uint8_t {
        STATUS,
        SET_JSON
    };

    Type type;
    uint8_t dp;
    char value[96];
};

TinyTuyaAsync::TinyTuyaAsync()
    : queue_(nullptr),
      task_(nullptr),
      running_(false),
      busy_(false),
      last_error_(TUYA_OK),
      callback_(nullptr),
      callback_user_(nullptr) {
    memset(&dev_, 0, sizeof(dev_));
}

TinyTuyaAsync::~TinyTuyaAsync() {
    stop();
}

tuya_err_t TinyTuyaAsync::begin(const char *deviceId,
                                const char *ip,
                                const char *localKey,
                                float version,
                                tuya_event_cb_t callback,
                                void *user,
                                uint32_t taskStack,
                                uint8_t taskPriority) {
    if (running_) return TUYA_ERR_BUSY;

    tuya_err_t err = tuya_device_init(&dev_, deviceId, ip, localKey);
    if (err != TUYA_OK) return err;
    tuya_device_set_version(&dev_, version_from_float(version));
    tuya_device_set_persistent(&dev_, true);

    callback_ = callback;
    callback_user_ = user;
    last_error_ = TUYA_OK;
    busy_ = false;
    running_ = true;

    queue_ = xQueueCreate(4, sizeof(AsyncRequest));
    if (!queue_) {
        running_ = false;
        return TUYA_ERR_NOMEM;
    }

    TaskHandle_t handle = nullptr;
    BaseType_t ok = xTaskCreate(TinyTuyaAsync::taskEntry,
                                "TinyTuyaAsync",
                                taskStack,
                                this,
                                (UBaseType_t)taskPriority,
                                &handle);
    if (ok != pdPASS) {
        vQueueDelete((QueueHandle_t)queue_);
        queue_ = nullptr;
        running_ = false;
        return TUYA_ERR_NOMEM;
    }

    task_ = handle;
    return TUYA_OK;
}

tuya_err_t TinyTuyaAsync::enqueue(const AsyncRequest &request) {
    if (!running_ || !queue_) return TUYA_ERR_NOT_CONNECTED;
    BaseType_t ok = xQueueSend((QueueHandle_t)queue_, &request, 0);
    return ok == pdTRUE ? TUYA_OK : TUYA_ERR_BUSY;
}

tuya_err_t TinyTuyaAsync::requestStatus() {
    AsyncRequest request;
    memset(&request, 0, sizeof(request));
    request.type = AsyncRequest::STATUS;
    return enqueue(request);
}

tuya_err_t TinyTuyaAsync::requestSet(uint8_t dp, const char *jsonLiteral) {
    if (!jsonLiteral) return TUYA_ERR_INVAL;
    AsyncRequest request;
    memset(&request, 0, sizeof(request));
    request.type = AsyncRequest::SET_JSON;
    request.dp = dp;
    strncpy(request.value, jsonLiteral, sizeof(request.value) - 1U);
    return enqueue(request);
}

tuya_err_t TinyTuyaAsync::requestSetBool(uint8_t dp, bool value) {
    return requestSet(dp, value ? "true" : "false");
}

tuya_err_t TinyTuyaAsync::requestSetInt(uint8_t dp, int value) {
    char literal[24];
    snprintf(literal, sizeof(literal), "%d", value);
    return requestSet(dp, literal);
}

tuya_err_t TinyTuyaAsync::requestSetString(uint8_t dp, const char *value) {
    if (!value) return TUYA_ERR_INVAL;
    char literal[96];
    size_t off = 0;
    literal[off++] = '"';
    for (const char *p = value; *p && off + 3U < sizeof(literal); ++p) {
        if (*p == '"' || *p == '\\') literal[off++] = '\\';
        literal[off++] = *p;
    }
    if (off + 1U >= sizeof(literal)) return TUYA_ERR_BUFFER_TOO_SMALL;
    literal[off++] = '"';
    literal[off] = '\0';
    return requestSet(dp, literal);
}

bool TinyTuyaAsync::busy() const {
    QueueHandle_t queue = (QueueHandle_t)queue_;
    return busy_ || (queue && uxQueueMessagesWaiting(queue) > 0);
}

tuya_err_t TinyTuyaAsync::lastError() const {
    return last_error_;
}

void TinyTuyaAsync::stop() {
    running_ = false;

    TaskHandle_t task = (TaskHandle_t)task_;
    task_ = nullptr;
    if (task && task != xTaskGetCurrentTaskHandle()) {
        vTaskDelete(task);
    }

    if (queue_) {
        vQueueDelete((QueueHandle_t)queue_);
        queue_ = nullptr;
    }

    busy_ = false;
    tuya_device_close(&dev_);
}

void TinyTuyaAsync::taskEntry(void *arg) {
    TinyTuyaAsync *self = static_cast<TinyTuyaAsync *>(arg);
    if (self) self->run();
    vTaskDelete(nullptr);
}

void TinyTuyaAsync::run() {
    while (running_) {
        AsyncRequest request;
        if (xQueueReceive((QueueHandle_t)queue_, &request, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }

        busy_ = true;
        char response[TUYA_MAX_JSON_LENGTH];
        response[0] = '\0';
        tuya_err_t err = TUYA_ERR_INVAL;

        if (request.type == AsyncRequest::STATUS) {
            err = tuya_status(&dev_, response, sizeof(response));
        } else if (request.type == AsyncRequest::SET_JSON) {
            err = tuya_set_value_json(&dev_, request.dp, request.value, response, sizeof(response));
        }

        last_error_ = err;
        if (callback_) {
            if (err == TUYA_OK) {
                callback_(TUYA_EVENT_STATUS_RECEIVED,
                          response[0] ? response : nullptr,
                          callback_user_);
            } else {
                callback_(TUYA_EVENT_ERROR, nullptr, callback_user_);
            }
        }
        busy_ = false;
    }
}

struct TinyTuyaMulti::Slot {
    bool used;
    bool auto_poll;
    uint32_t poll_ms;
    uint32_t next_poll_ms;
    tuya_err_t last_error;
    TinyTuyaMulti *owner;
    size_t index;
    tuya_device_t dev;
    tuya_fsm_t fsm;
};

TinyTuyaMulti::TinyTuyaMulti(size_t capacity)
    : slots_(nullptr),
      capacity_(capacity),
      count_(0),
      cursor_(0),
      callback_(nullptr),
      callback_user_(nullptr) {
    if (capacity_ > 0) {
        slots_ = static_cast<Slot *>(calloc(capacity_, sizeof(Slot)));
    }
}

TinyTuyaMulti::~TinyTuyaMulti() {
    disconnectAll();
    free(slots_);
}

bool TinyTuyaMulti::ok() const {
    return slots_ != nullptr || capacity_ == 0;
}

size_t TinyTuyaMulti::capacity() const {
    return capacity_;
}

size_t TinyTuyaMulti::count() const {
    return count_;
}

TinyTuyaMulti::Slot *TinyTuyaMulti::slotAt(size_t index) const {
    if (!slots_ || index >= count_ || !slots_[index].used) return nullptr;
    return &slots_[index];
}

int TinyTuyaMulti::addDevice(const char *deviceId,
                             const char *ip,
                             const char *localKey,
                             float version,
                             uint32_t pollMs) {
    if (!slots_ || count_ >= capacity_) return -1;
    Slot *slot = &slots_[count_];
    memset(slot, 0, sizeof(*slot));

    tuya_err_t err = tuya_device_init(&slot->dev, deviceId, ip, localKey);
    if (err != TUYA_OK) return -1;
    tuya_device_set_version(&slot->dev, version_from_float(version));
    tuya_device_set_persistent(&slot->dev, true);

    slot->used = true;
    slot->auto_poll = pollMs > 0;
    slot->poll_ms = pollMs;
    slot->next_poll_ms = millis();
    slot->last_error = TUYA_OK;
    slot->owner = this;
    slot->index = count_;
    tuya_fsm_init(&slot->fsm, &slot->dev, TinyTuyaMulti::onFsmEvent, slot);
    ++count_;
    return (int)slot->index;
}

void TinyTuyaMulti::setCallback(tiny_tuya_multi_cb_t callback, void *user) {
    callback_ = callback;
    callback_user_ = user;
}

tuya_err_t TinyTuyaMulti::setPollInterval(size_t index, uint32_t pollMs) {
    Slot *slot = slotAt(index);
    if (!slot) return TUYA_ERR_INVAL;
    slot->auto_poll = pollMs > 0;
    slot->poll_ms = pollMs;
    slot->next_poll_ms = millis() + pollMs;
    return TUYA_OK;
}

tuya_err_t TinyTuyaMulti::requestStatus(size_t index) {
    Slot *slot = slotAt(index);
    if (!slot) return TUYA_ERR_INVAL;
    if (slot->fsm.pending != TUYA_FSM_REQ_NONE) return TUYA_ERR_BUSY;
    tuya_fsm_request_status(&slot->fsm);
    if (slot->auto_poll) slot->next_poll_ms = millis() + slot->poll_ms;
    return TUYA_OK;
}

tuya_err_t TinyTuyaMulti::requestSet(size_t index, uint8_t dp, const char *jsonLiteral) {
    Slot *slot = slotAt(index);
    if (!slot || !jsonLiteral) return TUYA_ERR_INVAL;
    if (slot->fsm.pending != TUYA_FSM_REQ_NONE) return TUYA_ERR_BUSY;
    tuya_fsm_request_set(&slot->fsm, dp, jsonLiteral);
    if (slot->auto_poll) slot->next_poll_ms = millis() + slot->poll_ms;
    return TUYA_OK;
}

tuya_err_t TinyTuyaMulti::requestSetBool(size_t index, uint8_t dp, bool value) {
    return requestSet(index, dp, value ? "true" : "false");
}

tuya_err_t TinyTuyaMulti::requestSetInt(size_t index, uint8_t dp, int value) {
    char literal[24];
    snprintf(literal, sizeof(literal), "%d", value);
    return requestSet(index, dp, literal);
}

tuya_err_t TinyTuyaMulti::requestSetString(size_t index, uint8_t dp, const char *value) {
    if (!value) return TUYA_ERR_INVAL;
    char literal[96];
    size_t off = 0;
    literal[off++] = '"';
    for (const char *p = value; *p && off + 3U < sizeof(literal); ++p) {
        if (*p == '"' || *p == '\\') literal[off++] = '\\';
        literal[off++] = *p;
    }
    if (off + 1U >= sizeof(literal)) return TUYA_ERR_BUFFER_TOO_SMALL;
    literal[off++] = '"';
    literal[off] = '\0';
    return requestSet(index, dp, literal);
}

void TinyTuyaMulti::loop() {
    if (!slots_ || count_ == 0) return;
    uint32_t now = millis();

    for (size_t i = 0; i < count_; ++i) {
        size_t index = (cursor_ + i) % count_;
        Slot *slot = slotAt(index);
        if (!slot) continue;

        if (slot->fsm.pending == TUYA_FSM_REQ_NONE &&
            slot->auto_poll &&
            (int32_t)(now - slot->next_poll_ms) >= 0) {
            tuya_fsm_request_status(&slot->fsm);
            slot->next_poll_ms = now + slot->poll_ms;
        }

        if (slot->fsm.pending != TUYA_FSM_REQ_NONE) {
            cursor_ = (index + 1U) % count_;
            tuya_fsm_loop(&slot->fsm);
            slot->last_error = slot->fsm.last_error;
            return;
        }
    }
}

void TinyTuyaMulti::disconnect(size_t index) {
    Slot *slot = slotAt(index);
    if (slot) tuya_fsm_disconnect(&slot->fsm);
}

void TinyTuyaMulti::disconnectAll() {
    if (!slots_) return;
    for (size_t i = 0; i < count_; ++i) {
        if (slots_[i].used) tuya_fsm_disconnect(&slots_[i].fsm);
    }
}

tuya_err_t TinyTuyaMulti::lastError(size_t index) const {
    Slot *slot = slotAt(index);
    return slot ? slot->last_error : TUYA_ERR_INVAL;
}

tuya_fsm_state_t TinyTuyaMulti::state(size_t index) const {
    Slot *slot = slotAt(index);
    return slot ? slot->fsm.state : TUYA_FSM_ERROR;
}

const char *TinyTuyaMulti::deviceId(size_t index) const {
    Slot *slot = slotAt(index);
    return slot ? slot->dev.id : nullptr;
}

tuya_device_t *TinyTuyaMulti::raw(size_t index) {
    Slot *slot = slotAt(index);
    return slot ? &slot->dev : nullptr;
}

void TinyTuyaMulti::onFsmEvent(tuya_event_t event, const char *payload, void *user) {
    Slot *slot = static_cast<Slot *>(user);
    if (!slot || !slot->owner) return;
    slot->last_error = slot->fsm.last_error;
    if (slot->owner->callback_) {
        slot->owner->callback_(slot->index,
                               slot->dev.id,
                               event,
                               payload,
                               slot->last_error,
                               slot->owner->callback_user_);
    }
}

#endif
