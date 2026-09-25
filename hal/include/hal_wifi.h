#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_WIFI_MODE_STA = 0,
    HAL_WIFI_MODE_AP,
    HAL_WIFI_MODE_STA_AP
} hal_wifi_mode_t;

typedef struct hal_wifi hal_wifi_t;

typedef struct {
    char ssid[32];
    char password[64];
    int8_t rssi;
    uint8_t channel;
    uint8_t bssid[6];
    bool connected;
} hal_wifi_ap_t;

#define HAL_WIFI_MAX_APS 16

typedef void (*hal_wifi_scan_cb_t)(const hal_wifi_ap_t* aps, int count, void* arg);
typedef void (*hal_wifi_event_cb_t)(int event, void* arg);

hal_wifi_t* hal_wifi_open(const char* path);
void hal_wifi_close(hal_wifi_t* wifi);

int hal_wifi_init(hal_wifi_t* wifi);
int hal_wifi_start(hal_wifi_t* wifi);
int hal_wifi_stop(hal_wifi_t* wifi);

int hal_wifi_set_mode(hal_wifi_t* wifi, hal_wifi_mode_t mode);
hal_wifi_mode_t hal_wifi_get_mode(const hal_wifi_t* wifi);

int hal_wifi_connect(hal_wifi_t* wifi, const char* ssid, const char* password);
int hal_wifi_disconnect(hal_wifi_t* wifi);

bool hal_wifi_is_connected(const hal_wifi_t* wifi);
int hal_wifi_get_rssi(const hal_wifi_t* wifi);

int hal_wifi_scan(hal_wifi_t* wifi, hal_wifi_scan_cb_t cb, void* arg);

void hal_wifi_set_event_callback(hal_wifi_t* wifi, hal_wifi_event_cb_t cb, void* arg);

void hal_wifi_add_ap(const char* ssid, const char* password, int8_t rssi);

const char* hal_wifi_get_path(const hal_wifi_t* wifi);

#ifdef __cplusplus
}
#endif