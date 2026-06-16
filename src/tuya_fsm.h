#ifndef TUYA_FSM_H
#define TUYA_FSM_H

#include "tuya_device.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TUYA_FSM_IDLE,
    TUYA_FSM_READY,
    TUYA_FSM_SENDING,
    TUYA_FSM_WAITING_RESPONSE,
    TUYA_FSM_DISCONNECTED,
    TUYA_FSM_ERROR
} tuya_fsm_state_t;

typedef enum {
    TUYA_FSM_REQ_NONE,
    TUYA_FSM_REQ_STATUS,
    TUYA_FSM_REQ_SET_JSON
} tuya_fsm_request_t;

typedef struct {
    tuya_device_t *dev;
    tuya_fsm_state_t state;
    uint32_t state_entered_ms;
    tuya_fsm_request_t pending;
    uint8_t pending_dp;
    char pending_value[96];
    char response[TUYA_MAX_JSON_LENGTH];
    tuya_event_cb_t callback;
    void *user;
} tuya_fsm_t;

void tuya_fsm_init(tuya_fsm_t *fsm, tuya_device_t *dev, tuya_event_cb_t cb, void *user);
void tuya_fsm_request_status(tuya_fsm_t *fsm);
void tuya_fsm_request_set(tuya_fsm_t *fsm, uint8_t dp, const char *value_json);
void tuya_fsm_loop(tuya_fsm_t *fsm);
void tuya_fsm_disconnect(tuya_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif
