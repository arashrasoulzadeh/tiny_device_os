#include "hal_wifi.h"
#include <stdlib.h>
#include <string.h>

#define HAL_WIFI_MAX_APS 16

struct hal_wifi {
    char path[64];
    hal_wifi_mode_t mode;
    bool initialized;
    bool started;
    bool connected;
    char ssid[32];
    char password[64];
    int8_t rssi;
    hal_wifi_ap_t aps[HAL_WIFI_MAX_APS];
    int ap_count;
    hal_wifi_scan_cb_t scan_cb;
    void* scan_arg;
    hal_wifi_event_cb_t event_cb;
    void* event_arg;
};

hal_wifi_t* hal_wifi_open(const char* path) {
    hal_wifi_t* wifi = calloc(1, sizeof(hal_wifi_t));
    if (!wifi) return NULL;
    
    strncpy(wifi->path, path, sizeof(wifi->path) - 1);
    wifi->mode = HAL_WIFI_MODE_STA;
    wifi->initialized = false;
    wifi->started = false;
    wifi->connected = false;
    wifi->ap_count = 0;
    wifi->rssi = -99;
    
    return wifi;
}

void hal_wifi_close(hal_wifi_t* wifi) {
    if (wifi) free(wifi);
}

int hal_wifi_init(hal_wifi_t* wifi) {
    if (!wifi) return -1;
    wifi->initialized = true;
    return 0;
}

int hal_wifi_start(hal_wifi_t* wifi) {
    if (!wifi || !wifi->initialized) return -1;
    wifi->started = true;
    return 0;
}

int hal_wifi_stop(hal_wifi_t* wifi) {
    if (!wifi) return -1;
    wifi->started = false;
    wifi->connected = false;
    return 0;
}

int hal_wifi_set_mode(hal_wifi_t* wifi, hal_wifi_mode_t mode) {
    if (!wifi) return -1;
    wifi->mode = mode;
    return 0;
}

hal_wifi_mode_t hal_wifi_get_mode(const hal_wifi_t* wifi) {
    return wifi ? wifi->mode : HAL_WIFI_MODE_STA;
}

int hal_wifi_connect(hal_wifi_t* wifi, const char* ssid, const char* password) {
    if (!wifi || !ssid) return -1;
    
    strncpy(wifi->ssid, ssid, sizeof(wifi->ssid) - 1);
    if (password) {
        strncpy(wifi->password, password, sizeof(wifi->password) - 1);
    }
    wifi->connected = true;
    wifi->rssi = -50;
    
    if (wifi->event_cb) {
        wifi->event_cb(1, wifi->event_arg); // Connected event
    }
    
    return 0;
}

int hal_wifi_disconnect(hal_wifi_t* wifi) {
    if (!wifi) return -1;
    wifi->connected = false;
    
    if (wifi->event_cb) {
        wifi->event_cb(2, wifi->event_arg); // Disconnected event
    }
    
    return 0;
}

bool hal_wifi_is_connected(const hal_wifi_t* wifi) {
    return wifi ? wifi->connected : false;
}

int hal_wifi_get_rssi(const hal_wifi_t* wifi) {
    return wifi ? wifi->rssi : -99;
}

int hal_wifi_scan(hal_wifi_t* wifi, hal_wifi_scan_cb_t cb, void* arg) {
    if (!wifi) return -1;
    if (cb) {
        cb(wifi->aps, wifi->ap_count, arg);
    }
    return 0;
}

void hal_wifi_set_event_callback(hal_wifi_t* wifi, hal_wifi_event_cb_t cb, void* arg) {
    if (!wifi) return;
    wifi->event_cb = cb;
    wifi->event_arg = arg;
}

void hal_wifi_add_ap(const char* ssid, const char* password, int8_t rssi) {
    // Simulator implementation would add to a global list
    (void)ssid; (void)password; (void)rssi;
}

const char* hal_wifi_get_path(const hal_wifi_t* wifi) {
    return wifi ? wifi->path : NULL;
}

int hal_wifi_suspend(hal_wifi_t* wifi) {
    if (!wifi) return -1;
    wifi->started = false;
    wifi->connected = false;
    return 0;
}

int hal_wifi_resume(hal_wifi_t* wifi) {
    if (!wifi) return -1;
    wifi->started = true;
    return 0;
}