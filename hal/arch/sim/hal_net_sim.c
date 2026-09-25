#include "hal_net.h"
#include <stdlib.h>
#include <string.h>

struct hal_net {
    char path[64];
    hal_net_type_t type;
    bool connected;
    bool has_ip;
    hal_net_ipv4_config_t ipv4;
    hal_net_ipv6_config_t ipv6;
    hal_net_event_cb_t event_cb;
    void* event_arg;
};

hal_net_t* hal_net_open(const char* path, hal_net_type_t type) {
    hal_net_t* net = calloc(1, sizeof(hal_net_t));
    if (!net) return NULL;
    
    strncpy(net->path, path, sizeof(net->path) - 1);
    net->type = type;
    net->connected = false;
    net->has_ip = false;
    
    memset(&net->ipv4, 0, sizeof(net->ipv4));
    memset(&net->ipv6, 0, sizeof(net->ipv6));
    net->ipv4.dhcp = true;
    net->ipv6.dhcp = true;
    
    return net;
}

void hal_net_close(hal_net_t* net) {
    if (net) free(net);
}

int hal_net_start(hal_net_t* net) {
    if (!net) return -1;
    net->connected = true;
    net->has_ip = true;
    
    if (net->event_cb) {
        net->event_cb(net, HAL_NET_EVENT_CONNECTED, net->event_arg);
        net->event_cb(net, HAL_NET_EVENT_GOT_IP, net->event_arg);
    }
    return 0;
}

int hal_net_stop(hal_net_t* net) {
    if (!net) return -1;
    net->connected = false;
    net->has_ip = false;
    
    if (net->event_cb) {
        net->event_cb(net, HAL_NET_EVENT_DISCONNECTED, net->event_arg);
        net->event_cb(net, HAL_NET_EVENT_LOST_IP, net->event_arg);
    }
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
    return (int)len;
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