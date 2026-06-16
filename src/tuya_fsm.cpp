#include "tuya_fsm.h"

#include <Arduino.h>
#include <string.h>

static void enter_state(tuya_fsm_t *fsm, tuya_fsm_state_t state) {
    fsm->state = state;
    fsm->state_entered_ms = millis();
}

void tuya_fsm_init(tuya_fsm_t *fsm, tuya_device_t *dev, tuya_event_cb_t cb, void *user) {
    if (!fsm) return;
    memset(fsm, 0, sizeof(*fsm));
    fsm->dev = dev;
    fsm->callback = cb;
    fsm->user = user;
    enter_state(fsm, TUYA_FSM_IDLE);
}

void tuya_fsm_request_status(tuya_fsm_t *fsm) {
    if (!fsm || fsm->pending != TUYA_FSM_REQ_NONE) return;
    fsm->pending = TUYA_FSM_REQ_STATUS;
}

void tuya_fsm_request_set(tuya_fsm_t *fsm, uint8_t dp, const char *value_json) {
    if (!fsm || !value_json || fsm->pending != TUYA_FSM_REQ_NONE) return;
    fsm->pending = TUYA_FSM_REQ_SET_JSON;
    fsm->pending_dp = dp;
    strncpy(fsm->pending_value, value_json, sizeof(fsm->pending_value) - 1U);
    fsm->pending_value[sizeof(fsm->pending_value) - 1U] = '\0';
}

void tuya_fsm_loop(tuya_fsm_t *fsm) {
    if (!fsm || !fsm->dev || fsm->pending == TUYA_FSM_REQ_NONE) return;
    enter_state(fsm, TUYA_FSM_SENDING);
    tuya_err_t err;
    if (fsm->pending == TUYA_FSM_REQ_STATUS) {
        err = tuya_status(fsm->dev, fsm->response, sizeof(fsm->response));
    } else {
        err = tuya_set_value_json(fsm->dev, fsm->pending_dp, fsm->pending_value,
                                  fsm->response, sizeof(fsm->response));
    }
    fsm->pending = TUYA_FSM_REQ_NONE;
    if (err == TUYA_OK) {
        enter_state(fsm, TUYA_FSM_READY);
        if (fsm->callback) fsm->callback(TUYA_EVENT_STATUS_RECEIVED, fsm->response, fsm->user);
    } else {
        enter_state(fsm, TUYA_FSM_ERROR);
        if (fsm->callback) fsm->callback(TUYA_EVENT_ERROR, nullptr, fsm->user);
    }
}

void tuya_fsm_disconnect(tuya_fsm_t *fsm) {
    if (!fsm) return;
    if (fsm->dev) tuya_device_close(fsm->dev);
    enter_state(fsm, TUYA_FSM_DISCONNECTED);
}
