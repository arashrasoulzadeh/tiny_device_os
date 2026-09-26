#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "driver.h"
#include "hal_i2c.h"
#include "hal_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HOTPLUG_EVENT_MAX 32

typedef enum {
    HOTPLUG_EVENT_ADD = 0,
    HOTPLUG_EVENT_REMOVE,
    HOTPLUG_EVENT_CHANGE
} hotplug_event_type_t;

typedef struct {
    hotplug_event_type_t type;
    char device_path[64];
    char driver_name[DRIVER_NAME_MAX];
    void* bus_data;
    uint32_t timestamp;
} hotplug_event_t;

typedef void (*hotplug_callback_t)(const hotplug_event_t* event, void* arg);

typedef struct {
    device_t** devices;
    size_t capacity;
    size_t count;
    hotplug_callback_t hotplug_cb;
    void* hotplug_arg;
} device_registry_t;

int device_registry_init(void);
void device_registry_deinit(void);

int device_registry_add(device_t* dev);
int device_registry_remove(const char* path);
device_t* device_registry_find(const char* path);
device_t* device_registry_find_by_name(const char* name);

int device_registry_list(device_t*** list, size_t* count);

void device_registry_set_hotplug_callback(hotplug_callback_t cb, void* arg);
void device_registry_emit_hotplug(hotplug_event_type_t type, const char* path, 
                                   const char* driver_name, void* bus_data);

int i2c_scan_bus(hal_i2c_t* i2c, uint8_t* addrs, size_t max_addrs, size_t* found);
int spi_scan_bus(hal_spi_t* spi);

int hotplug_scan_i2c(hal_i2c_t* i2c);
int hotplug_scan_spi(hal_spi_t* spi);

#ifdef __cplusplus
}
#endif