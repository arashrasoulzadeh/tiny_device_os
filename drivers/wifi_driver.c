#include "driver.h"
#include "hal_wifi.h"
#include "hal_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct wifi_device_data {
    hal_wifi_t* hal_wifi;
    hal_net_t* hal_net;
    bool connected;
};

static int wifi_probe(device_t* dev) {
    struct wifi_device_data* data = calloc(1, sizeof(struct wifi_device_data));
    if (!data) return -1;
    
    data->hal_wifi = hal_wifi_open("/dev/wifi0");
    if (!data->hal_wifi) {
        free(data);
        return -1;
    }
    
    if (hal_wifi_init(data->hal_wifi) != 0) {
        hal_wifi_close(data->hal_wifi);
        free(data);
        return -1;
    }
    
    data->hal_net = hal_net_open("/dev/net0", HAL_NET_TYPE_WIFI_STA);
    if (data->hal_net) {
        hal_net_start(data->hal_net);
    }
    
    dev->private_data = data;
    return 0;
}

static int wifi_remove(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    
    struct wifi_device_data* data = (struct wifi_device_data*)dev->private_data;
    if (data->hal_wifi) {
        hal_wifi_stop(data->hal_wifi);
        hal_wifi_close(data->hal_wifi);
    }
    if (data->hal_net) {
        hal_net_stop(data->hal_net);
        hal_net_close(data->hal_net);
    }
    free(data);
    dev->private_data = NULL;
    return 0;
}

static int wifi_open(device_t* dev, void** handle) {
    if (!dev || !dev->private_data) return -1;
    *handle = dev->private_data;
    return 0;
}

static int wifi_close(void* handle) {
    (void)handle;
    return 0;
}

static ssize_t wifi_read(void* handle, void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    struct wifi_device_data* data = (struct wifi_device_data*)handle;
    if (!data->hal_net) return -1;
    
    return hal_net_recv(data->hal_net, buf, count);
}

static ssize_t wifi_write(void* handle, const void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    struct wifi_device_data* data = (struct wifi_device_data*)handle;
    if (!data->hal_net) return -1;
    
    return hal_net_send(data->hal_net, buf, count);
}

static int wifi_ioctl(void* handle, uint32_t cmd, void* arg) {
    if (!handle) return -1;
    
    struct wifi_device_data* data = (struct wifi_device_data*)handle;
    
    switch (cmd) {
        case 0x40:  // WIFI_SET_MODE
            if (arg && data->hal_wifi) {
                return hal_wifi_set_mode(data->hal_wifi, *(hal_wifi_mode_t*)arg);
            }
            break;
        case 0x41:  // WIFI_GET_MODE
            if (arg && data->hal_wifi) {
                *(hal_wifi_mode_t*)arg = hal_wifi_get_mode(data->hal_wifi);
                return 0;
            }
            break;
        case 0x42:  // WIFI_CONNECT
            if (arg && data->hal_wifi) {
                // arg should be a struct with ssid/password
                return 0;
            }
            break;
        case 0x43:  // WIFI_DISCONNECT
            if (data->hal_wifi) {
                data->connected = false;
                return hal_wifi_disconnect(data->hal_wifi);
            }
            break;
        case 0x44:  // WIFI_IS_CONNECTED
            if (arg && data->hal_wifi) {
                *(bool*)arg = hal_wifi_is_connected(data->hal_wifi);
                return 0;
            }
            break;
        case 0x45:  // WIFI_GET_RSSI
            if (arg && data->hal_wifi) {
                *(int*)arg = hal_wifi_get_rssi(data->hal_wifi);
                return 0;
            }
            break;
        case 0x46:  // WIFI_SCAN
            if (arg && data->hal_wifi) {
                // Would implement scan
                return 0;
            }
            break;
    }
    return -1;
}

static int wifi_suspend(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct wifi_device_data* data = (struct wifi_device_data*)dev->private_data;
    int ret = 0;
    if (data->hal_wifi) ret |= hal_wifi_suspend(data->hal_wifi);
    if (data->hal_net) ret |= hal_net_suspend(data->hal_net);
    return ret;
}

static int wifi_resume(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct wifi_device_data* data = (struct wifi_device_data*)dev->private_data;
    int ret = 0;
    if (data->hal_wifi) ret |= hal_wifi_resume(data->hal_wifi);
    if (data->hal_net) ret |= hal_net_resume(data->hal_net);
    return ret;
}

static const driver_ops_t wifi_ops = {
    .probe = wifi_probe,
    .remove = wifi_remove,
    .open = wifi_open,
    .close = wifi_close,
    .read = wifi_read,
    .write = wifi_write,
    .ioctl = wifi_ioctl,
    .suspend = wifi_suspend,
    .resume = wifi_resume,
};

static driver_t g_wifi_driver = {
    .name = "wifi",
    .type = DRIVER_TYPE_NETWORK,
    .ops = &wifi_ops,
};

int wifi_driver_init(void) {
    return driver_register(&g_wifi_driver);
}

void wifi_driver_deinit(void) {
    driver_unregister("wifi");
}

int wifi_create_device(const char* name) {
    device_t* dev = calloc(1, sizeof(device_t));
    if (!dev) return -1;
    
    if (name) {
        strncpy(dev->name, name, DEVICE_NAME_MAX - 1);
    } else {
        strncpy(dev->name, "wifi0", DEVICE_NAME_MAX - 1);
    }
    
    strncpy(dev->path, "/dev/wifi0", sizeof(dev->path) - 1);
    dev->driver = &g_wifi_driver;
    
    return device_register(dev);
}