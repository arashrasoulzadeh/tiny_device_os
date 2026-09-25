#include "hal_net.h"
#include "hal_wifi.h"
#include "hal_power.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "hal_net";

typedef struct hal_net {
    char path[32];
    hal_net_type_t type;
    bool connected;
    bool has_ip;
    hal_net_ipv4_config_t ipv4;
    hal_net_ipv6_config_t ipv6;
    hal_net_event_cb_t event_cb;
    void* event_arg;
    esp_netif_t* netif;
} hal_net_t;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    hal_net_t* net = (hal_net_t*)arg;
    
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            net->connected = false;
            net->has_ip = false;
            if (net->event_cb) {
                net->event_cb(net, HAL_NET_EVENT_DISCONNECTED, net->event_arg);
                net->event_cb(net, HAL_NET_EVENT_LOST_IP, net->event_arg);
            }
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            net->connected = true;
            net->has_ip = true;
            memcpy(net->ipv4.ip, &event->ip_info.ip, 4);
            memcpy(net->ipv4.netmask, &event->ip_info.netmask, 4);
            memcpy(net->ipv4.gateway, &event->ip_info.gw, 4);
            
            if (net->event_cb) {
                net->event_cb(net, HAL_NET_EVENT_CONNECTED, net->event_arg);
                net->event_cb(net, HAL_NET_EVENT_GOT_IP, net->event_arg);
            }
        }
    }
}

hal_net_t* hal_net_open(const char* path, hal_net_type_t type) {
    hal_net_t* net = calloc(1, sizeof(hal_net_t));
    if (!net) return NULL;
    
    strncpy(net->path, path, sizeof(net->path) - 1);
    net->type = type;
    net->connected = false;
    net->has_ip = false;
    net->ipv4.dhcp = true;
    net->ipv6.dhcp = true;
    
    static bool wifi_init = false;
    if (!wifi_init && (type == HAL_NET_TYPE_WIFI_STA || type == HAL_NET_TYPE_WIFI_AP)) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        
        if (type == HAL_NET_TYPE_WIFI_STA) {
            net->netif = esp_netif_create_default_wifi_sta();
        } else if (type == HAL_NET_TYPE_WIFI_AP) {
            net->netif = esp_netif_create_default_wifi_ap();
        }
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                             &wifi_event_handler, net, &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                                             &wifi_event_handler, net, &instance_got_ip));
        
        wifi_init = true;
    }
    
    return net;
}

void hal_net_close(hal_net_t* net) {
    if (!net) return;
    if (net->type == HAL_NET_TYPE_WIFI_STA || net->type == HAL_NET_TYPE_WIFI_AP) {
        esp_wifi_stop();
        esp_wifi_deinit();
    }
    free(net);
}

int hal_net_start(hal_net_t* net) {
    if (!net) return -1;
    
    if (net->type == HAL_NET_TYPE_WIFI_STA) {
        wifi_config_t wifi_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = { .capable = true, .required = false },
            },
        };
        strncpy((char*)wifi_config.sta.ssid, (char*)net->ipv4.ip, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, (char*)net->ipv4.netmask, sizeof(wifi_config.sta.password) - 1);
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        
    } else if (net->type == HAL_NET_TYPE_WIFI_AP) {
        wifi_config_t wifi_config = {
            .ap = {
                .ssid_len = 0,
                .channel = 1,
                .password = "",
                .max_connection = 4,
                .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            },
        };
        strncpy((char*)wifi_config.ap.ssid, (char*)net->ipv4.ip, sizeof(wifi_config.ap.ssid) - 1);
        strncpy((char*)wifi_config.ap.password, (char*)net->ipv4.netmask, sizeof(wifi_config.ap.password) - 1);
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    
    return 0;
}

int hal_net_stop(hal_net_t* net) {
    if (!net) return -1;
    esp_wifi_stop();
    return 0;
}

int hal_net_set_ipv4_config(hal_net_t* net, const hal_net_ipv4_config_t* config) {
    if (!net || !config) return -1;
    net->ipv4 = *config;
    return 0;
}

int hal_net_get_ipv4_config(hal_net_t* net, hal_net_ipv4_config_t* config) {
    if (!net || !config) return -1;
    *config = net->ipv4;
    return 0;
}

int hal_net_set_ipv6_config(hal_net_t* net, const hal_net_ipv6_config_t* config) {
    if (!net || !config) return -1;
    net->ipv6 = *config;
    return 0;
}

int hal_net_get_ipv6_config(hal_net_t* net, hal_net_ipv6_config_t* config) {
    if (!net || !config) return -1;
    *config = net->ipv6;
    return 0;
}

int hal_net_set_event_callback(hal_net_t* net, hal_net_event_cb_t cb, void* arg) {
    if (!net) return -1;
    net->event_cb = cb;
    net->event_arg = arg;
    return 0;
}

bool hal_net_is_connected(const hal_net_t* net) {
    return net ? net->connected : false;
}

bool hal_net_has_ip(const hal_net_t* net) {
    return net ? net->has_ip : false;
}

int hal_net_send(hal_net_t* net, const void* data, size_t len) {
    (void)net; (void)data; (void)len;
    return 0;
}

int hal_net_recv(hal_net_t* net, void* data, size_t len) {
    (void)net; (void)data; (void)len;
    return 0;
}

const char* hal_net_get_path(const hal_net_t* net) {
    return net ? net->path : NULL;
}

hal_net_type_t hal_net_get_type(const hal_net_t* net) {
    return net ? net->type : HAL_NET_TYPE_NONE;
}

int hal_net_suspend(hal_net_t* net) {
    if (!net) return -1;
    net->connected = false;
    net->has_ip = false;
    return 0;
}

int hal_net_resume(hal_net_t* net) {
    if (!net) return -1;
    net->connected = true;
    net->has_ip = true;
    return 0;
}