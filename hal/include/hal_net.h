#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_NET_TYPE_NONE = 0,
    HAL_NET_TYPE_ETHERNET,
    HAL_NET_TYPE_WIFI_STA,
    HAL_NET_TYPE_WIFI_AP,
    HAL_NET_TYPE_BLUETOOTH,
    HAL_NET_TYPE_LORA,
    HAL_NET_TYPE_CELLULAR
} hal_net_type_t;

typedef enum {
    HAL_NET_IPV4 = 0,
    HAL_NET_IPV6
} hal_net_ip_version_t;

typedef struct hal_net hal_net_t;

typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t netmask[4];
    uint8_t gateway[4];
    uint8_t dns1[4];
    uint8_t dns2[4];
    bool dhcp;
} hal_net_ipv4_config_t;

typedef struct {
    uint8_t ip[16];
    uint8_t prefix_len;
    uint8_t gateway[16];
    uint8_t dns1[16];
    uint8_t dns2[16];
    bool dhcp;
} hal_net_ipv6_config_t;

typedef void (*hal_net_event_cb_t)(hal_net_t* net, int event, void* arg);

typedef enum {
    HAL_NET_EVENT_CONNECTED = 1,
    HAL_NET_EVENT_DISCONNECTED,
    HAL_NET_EVENT_GOT_IP,
    HAL_NET_EVENT_LOST_IP
} hal_net_event_t;

hal_net_t* hal_net_open(const char* path, hal_net_type_t type);
void hal_net_close(hal_net_t* net);

int hal_net_start(hal_net_t* net);
int hal_net_stop(hal_net_t* net);

int hal_net_set_ipv4_config(hal_net_t* net, const hal_net_ipv4_config_t* config);
int hal_net_get_ipv4_config(hal_net_t* net, hal_net_ipv4_config_t* config);

int hal_net_set_ipv6_config(hal_net_t* net, const hal_net_ipv6_config_t* config);
int hal_net_get_ipv6_config(hal_net_t* net, hal_net_ipv6_config_t* config);

int hal_net_set_event_callback(hal_net_t* net, hal_net_event_cb_t cb, void* arg);

bool hal_net_is_connected(const hal_net_t* net);
bool hal_net_has_ip(const hal_net_t* net);

int hal_net_send(hal_net_t* net, const void* data, size_t len);
int hal_net_recv(hal_net_t* net, void* data, size_t len);

const char* hal_net_get_path(const hal_net_t* net);
hal_net_type_t hal_net_get_type(const hal_net_t* net);

// Power management
int hal_net_suspend(hal_net_t* net);
int hal_net_resume(hal_net_t* net);

#ifdef __cplusplus
}
#endif