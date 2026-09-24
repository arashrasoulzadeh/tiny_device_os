#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIM_BLE_STATE_IDLE = 0,
    SIM_BLE_STATE_ADVERTISING,
    SIM_BLE_STATE_CONNECTED
} sim_ble_state_t;

typedef void (*sim_ble_event_cb_t)(int event, void* arg);

int sim_ble_init(void);
void sim_ble_cleanup(void);

int sim_ble_start_advertising(const char* name);
int sim_ble_stop_advertising(void);

sim_ble_state_t sim_ble_get_state(void);

void sim_ble_set_event_callback(sim_ble_event_cb_t cb, void* arg);

#ifdef __cplusplus
}
#endif