#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIM_WIFI_MODE_STA = 0,
    SIM_WIFI_MODE_AP,
    SIM_WIFI_MODE_STA_AP
} sim_wifi_mode_t;

typedef struct {
    char ssid[32];
    char password[64];
    int8_t rssi;
    uint8_t channel;
    uint8_t bssid[6];
    bool connected;
} sim_wifi_ap_t;

#define SIM_WIFI_MAX_APS 16

typedef void (*sim_wifi_scan_cb_t)(const sim_wifi_ap_t* aps, int count, void* arg);
typedef void (*sim_wifi_event_cb_t)(int event, void* arg);

int sim_wifi_init(void);
void sim_wifi_cleanup(void);

int sim_wifi_set_mode(sim_wifi_mode_t mode);
sim_wifi_mode_t sim_wifi_get_mode(void);

int sim_wifi_connect(const char* ssid, const char* password);
int sim_wifi_disconnect(void);

bool sim_wifi_is_connected(void);
int sim_wifi_get_rssi(void);

int sim_wifi_scan(sim_wifi_scan_cb_t cb, void* arg);

void sim_wifi_set_event_callback(sim_wifi_event_cb_t cb, void* arg);

void sim_wifi_add_ap(const char* ssid, const char* password, int8_t rssi);

#ifdef __cplusplus
}
#endif