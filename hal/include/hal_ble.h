#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_BLE_STATE_IDLE = 0,
    HAL_BLE_STATE_ADVERTISING,
    HAL_BLE_STATE_CONNECTED
} hal_ble_state_t;

typedef void (*hal_ble_event_cb_t)(int event, void* arg);

typedef struct hal_ble hal_ble_t;

hal_ble_t* hal_ble_open(const char* path);
void hal_ble_close(hal_ble_t* ble);

int hal_ble_init(hal_ble_t* ble);
int hal_ble_start(hal_ble_t* ble);
int hal_ble_stop(hal_ble_t* ble);

int hal_ble_start_advertising(const char* name);
int hal_ble_stop_advertising(void);

hal_ble_state_t hal_ble_get_state(void);

void hal_ble_set_event_callback(hal_ble_t* ble, void (*cb)(int event, void* arg), void* arg);

const char* hal_ble_get_path(const hal_ble_t* ble);

#ifdef __cplusplus
}
#endif