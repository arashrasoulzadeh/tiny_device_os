#include "hal_ble.h"
#include <stdlib.h>
#include <string.h>

struct hal_ble {
    char path[64];
    int state;
    bool initialized;
    bool started;
    hal_ble_event_cb_t event_cb;
    void* event_arg;
};

hal_ble_t* hal_ble_open(const char* path) {
    hal_ble_t* ble = calloc(1, sizeof(hal_ble_t));
    if (!ble) return NULL;
    
    strncpy(ble->path, path, sizeof(ble->path) - 1);
    ble->state = HAL_BLE_STATE_IDLE;
    ble->initialized = false;
    ble->started = false;
    
    return ble;
}

void hal_ble_close(hal_ble_t* ble) {
    if (ble) free(ble);
}

int hal_ble_init(hal_ble_t* ble) {
    if (!ble) return -1;
    ble->initialized = true;
    return 0;
}

int hal_ble_start(hal_ble_t* ble) {
    if (!ble || !ble->initialized) return -1;
    ble->started = true;
    ble->state = HAL_BLE_STATE_ADVERTISING;
    return 0;
}

int hal_ble_stop(hal_ble_t* ble) {
    if (!ble) return -1;
    ble->started = false;
    ble->state = HAL_BLE_STATE_IDLE;
    return 0;
}

int hal_ble_start_advertising(const char* name) {
    (void)name;
    return 0;
}

int hal_ble_stop_advertising(void) {
    return 0;
}

hal_ble_state_t hal_ble_get_state(void) {
    return HAL_BLE_STATE_IDLE;
}

void hal_ble_set_event_callback(hal_ble_t* ble, void (*cb)(int event, void* arg), void* arg) {
    if (!ble) return;
    ble->event_cb = (hal_ble_event_cb_t)cb;
    ble->event_arg = arg;
}

const char* hal_ble_get_path(const hal_ble_t* ble) {
    return ble ? ble->path : NULL;
}

int hal_ble_suspend(hal_ble_t* ble) {
    if (!ble) return -1;
    ble->started = false;
    ble->state = HAL_BLE_STATE_IDLE;
    return 0;
}

int hal_ble_resume(hal_ble_t* ble) {
    if (!ble) return -1;
    ble->started = true;
    ble->state = HAL_BLE_STATE_ADVERTISING;
    return 0;
}