#include "sim_ble.h"
#include <stdlib.h>

static sim_ble_state_t g_state = SIM_BLE_STATE_IDLE;
static sim_ble_event_cb_t g_event_cb = NULL;
static void* g_event_arg = NULL;

int sim_ble_init(void) {
    g_state = SIM_BLE_STATE_IDLE;
    return 0;
}

void sim_ble_cleanup(void) {
}

int sim_ble_start_advertising(const char* name) {
    (void)name;
    g_state = SIM_BLE_STATE_ADVERTISING;
    return 0;
}

int sim_ble_stop_advertising(void) {
    g_state = SIM_BLE_STATE_IDLE;
    return 0;
}

sim_ble_state_t sim_ble_get_state(void) {
    return g_state;
}

void sim_ble_set_event_callback(sim_ble_event_cb_t cb, void* arg) {
    g_event_cb = cb;
    g_event_arg = arg;
}