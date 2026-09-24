#include "sim_wifi.h"
#include <stdlib.h>
#include <string.h>

static sim_wifi_mode_t g_mode = SIM_WIFI_MODE_STA;
static bool g_connected = false;
static char g_ssid[32] = "";
static char g_password[64] = "";
static int8_t g_rssi = -50;
static sim_wifi_ap_t g_aps[SIM_WIFI_MAX_APS];
static int g_ap_count = 0;
static sim_wifi_event_cb_t g_event_cb = NULL;
static void* g_event_arg = NULL;

int sim_wifi_init(void) {
    g_mode = SIM_WIFI_MODE_STA;
    g_connected = false;
    g_ap_count = 0;
    memset(g_aps, 0, sizeof(g_aps));
    return 0;
}

void sim_wifi_cleanup(void) {
}

int sim_wifi_set_mode(sim_wifi_mode_t mode) {
    g_mode = mode;
    return 0;
}

sim_wifi_mode_t sim_wifi_get_mode(void) {
    return g_mode;
}

int sim_wifi_connect(const char* ssid, const char* password) {
    if (!ssid) return -1;
    strncpy(g_ssid, ssid, sizeof(g_ssid) - 1);
    if (password) strncpy(g_password, password, sizeof(g_password) - 1);
    g_connected = true;
    g_rssi = -40;
    
    if (g_event_cb) {
        g_event_cb(1, g_event_arg);
    }
    return 0;
}

int sim_wifi_disconnect(void) {
    g_connected = false;
    g_ssid[0] = '\0';
    
    if (g_event_cb) {
        g_event_cb(2, g_event_arg);
    }
    return 0;
}

bool sim_wifi_is_connected(void) {
    return g_connected;
}

int sim_wifi_get_rssi(void) {
    return g_rssi;
}

int sim_wifi_scan(sim_wifi_scan_cb_t cb, void* arg) {
    if (cb) {
        cb(g_aps, g_ap_count, arg);
    }
    return g_ap_count;
}

void sim_wifi_set_event_callback(sim_wifi_event_cb_t cb, void* arg) {
    g_event_cb = cb;
    g_event_arg = arg;
}

void sim_wifi_add_ap(const char* ssid, const char* password, int8_t rssi) {
    if (g_ap_count >= SIM_WIFI_MAX_APS) return;
    sim_wifi_ap_t* ap = &g_aps[g_ap_count++];
    strncpy(ap->ssid, ssid, sizeof(ap->ssid) - 1);
    if (password) strncpy(ap->password, password, sizeof(ap->password) - 1);
    ap->rssi = rssi;
    ap->connected = false;
}