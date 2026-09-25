#include "hal_ble.h"
#include "hal_power.h"
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_main.h>
#include <esp_gatt_common_api.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "hal_ble";

#define BLE_DEVICE_NAME_MAX 32

typedef struct hal_ble {
    char path[32];
    bool initialized;
    hal_ble_state_t state;
    hal_ble_event_cb_t event_cb;
    void* event_arg;
    char device_name[32];
} hal_ble_t;

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    // Handle GAP events
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
    // Handle GATTS events
}

hal_ble_t* hal_ble_open(const char* path) {
    hal_ble_t* ble = calloc(1, sizeof(hal_ble_t));
    if (!ble) return NULL;
    
    strncpy(ble->path, path, sizeof(ble->path) - 1);
    ble->initialized = false;
    ble->state = HAL_BLE_STATE_IDLE;
    
    return ble;
}

void hal_ble_close(hal_ble_t* ble) {
    if (!ble) return;
    hal_ble_stop(ble);
    free(ble);
}

int hal_ble_init(hal_ble_t* ble) {
    if (!ble || ble->initialized) return -1;
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    
    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    
    ble->initialized = true;
    return 0;
}

int hal_ble_start(hal_ble_t* ble) {
    (void)ble;
    return -1;
}

int hal_ble_stop(hal_ble_t* ble) {
    if (!ble) return -1;
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    ble->initialized = false;
    ble->state = HAL_BLE_STATE_IDLE;
    return 0;
}

int hal_ble_start_advertising(const char* name) {
    if (!name) return -1;
    
    esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x20,
        .adv_int_max = 0x40,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    
    esp_ble_gap_config_adv_data_raw((uint8_t*)name, strlen(name));
    esp_ble_gap_start_advertising(&adv_params);
    
    return 0;
}

int hal_ble_stop_advertising(void) {
    return esp_ble_gap_stop_advertising() == ESP_OK ? 0 : -1;
}

hal_ble_state_t hal_ble_get_state(void) {
    return HAL_BLE_STATE_IDLE;
}

void hal_ble_set_event_callback(void (*cb)(int event, void* arg), void* arg) {
    // Global callback - not implemented for per-instance
}

void hal_ble_set_event_callback(hal_ble_t* ble, hal_ble_event_cb_t cb, void* arg) {
    if (!ble) return;
    ble->event_cb = cb;
    ble->event_arg = arg;
}

const char* hal_ble_get_path(const hal_ble_t* ble) {
    return ble ? ble->path : NULL;
}