#include "hal_wifi.h"
#include "hal_power.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "hal_wifi";

#define MAX_SCAN_RESULTS 16

typedef struct hal_wifi {
    char path[32];
    wifi_mode_t mode;
    bool initialized;
    hal_wifi_event_cb_t event_cb;
    void* event_arg;
    wifi_ap_record_t scan_results[MAX_SCAN_RESULTS];
    uint16_t scan_count;
} hal_wifi_t;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    hal_wifi_t* wifi = (hal_wifi_t*)arg;
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                if (wifi->event_cb) {
                    wifi->event_cb(1, wifi->event_arg); // Connected
                }
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                esp_wifi_connect();
                if (wifi->event_cb) {
                    wifi->event_cb(2, wifi->event_arg); // Disconnected
                }
                break;
            case WIFI_EVENT_SCAN_DONE: {
                wifi_scan_config_t config = { .show_hidden = true };
                uint16_t number = MAX_SCAN_RESULTS;
                esp_wifi_scan_get_ap_records(&number, wifi->scan_results);
                wifi->scan_count = number;
                if (wifi->event_cb) {
                    wifi->event_cb(3, wifi->event_arg); // Scan done
                }
                break;
            }
            default:
                break;
        }
    }
}

hal_wifi_t* hal_wifi_open(const char* path) {
    hal_wifi_t* wifi = calloc(1, sizeof(hal_wifi_t));
    if (!wifi) return NULL;
    
    strncpy(wifi->path, path, sizeof(wifi->path) - 1);
    wifi->mode = WIFI_MODE_STA;
    wifi->initialized = false;
    wifi->scan_count = 0;
    
    return wifi;
}

void hal_wifi_close(hal_wifi_t* wifi) {
    if (!wifi) return;
    hal_wifi_stop(wifi);
    free(wifi);
}

int hal_wifi_init(hal_wifi_t* wifi) {
    if (!wifi || wifi->initialized) return -1;
    
    static bool wifi_init = false;
    if (!wifi_init) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                             &wifi_event_handler, wifi, NULL));
        
        wifi_init = true;
    }
    
    wifi->initialized = true;
    return 0;
}

int hal_wifi_start(hal_wifi_t* wifi) {
    if (!wifi || !wifi->initialized) return -1;
    
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK) return -1;
    
    return 0;
}

int hal_wifi_stop(hal_wifi_t* wifi) {
    if (!wifi) return -1;
    esp_wifi_stop();
    return 0;
}

int hal_wifi_set_mode(hal_wifi_t* wifi, hal_wifi_mode_t mode) {
    if (!wifi) return -1;
    wifi_mode_t esp_mode;
    switch (mode) {
        case HAL_WIFI_MODE_STA: wifi->mode = WIFI_MODE_STA; break;
        case HAL_WIFI_MODE_AP: wifi->mode = WIFI_MODE_AP; break;
        case HAL_WIFI_MODE_STA_AP: wifi->mode = WIFI_MODE_APSTA; break;
        default: return -1;
    }
    return esp_wifi_set_mode(wifi->mode);
}

hal_wifi_mode_t hal_wifi_get_mode(const hal_wifi_t* wifi) {
    if (!wifi) return HAL_WIFI_MODE_STA;
    switch (wifi->mode) {
        case WIFI_MODE_STA: return HAL_WIFI_MODE_STA;
        case WIFI_MODE_AP: return HAL_WIFI_MODE_AP;
        case WIFI_MODE_APSTA: return HAL_WIFI_MODE_STA_AP;
        default: return HAL_WIFI_MODE_STA;
    }
}

int hal_wifi_connect(hal_wifi_t* wifi, const char* ssid, const char* password) {
    if (!wifi || !ssid) return -1;
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = { .capable = true, .required = false },
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return -1;
    
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) return -1;
    
    return esp_wifi_connect() == ESP_OK ? 0 : -1;
}

int hal_wifi_disconnect(hal_wifi_t* wifi) {
    if (!wifi) return -1;
    return esp_wifi_disconnect() == ESP_OK ? 0 : -1;
}

bool hal_wifi_is_connected(const hal_wifi_t* wifi) {
    if (!wifi) return false;
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&wifi->scan_results[0]) == ESP_OK;
}

int hal_wifi_get_rssi(const hal_wifi_t* wifi) {
    if (!wifi) return -127;
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&wifi->scan_results[0]) == ESP_OK) {
        return wifi->scan_results[0].rssi;
    }
    return -127;
}

int hal_wifi_scan(hal_wifi_t* wifi, hal_wifi_scan_cb_t cb, void* arg) {
    if (!wifi) return -1;
    
    wifi_scan_config_t scan_config = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) return -1;
    
    // Results are delivered via callback in event handler
    if (cb) {
        cb(NULL, 0, arg);
    }
    return 0;
}

void hal_wifi_set_event_callback(hal_wifi_t* wifi, hal_wifi_event_cb_t cb, void* arg) {
    if (!wifi) return;
    wifi->event_cb = cb;
    wifi->event_arg = arg;
}

void hal_wifi_add_ap(hal_wifi_t* wifi, const char* ssid, const char* password, int8_t rssi) {
    // Not needed for ESP32 - handled by scan
    (void)wifi; (void)ssid; (void)password; (void)rssi;
}

const char* hal_wifi_get_path(const hal_wifi_t* wifi) {
    return wifi ? wifi->path : NULL;
}