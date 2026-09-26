#include "device_registry.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_CAPACITY 16

static device_registry_t g_registry = {0};

int device_registry_init(void) {
    g_registry.capacity = INITIAL_CAPACITY;
    g_registry.devices = calloc(INITIAL_CAPACITY, sizeof(device_t*));
    if (!g_registry.devices) return -1;
    g_registry.count = 0;
    g_registry.hotplug_cb = NULL;
    g_registry.hotplug_arg = NULL;
    return 0;
}

void device_registry_deinit(void) {
    free(g_registry.devices);
    g_registry.devices = NULL;
    g_registry.capacity = 0;
    g_registry.count = 0;
}

static int registry_resize(void) {
    size_t new_cap = g_registry.capacity * 2;
    device_t** new_devices = realloc(g_registry.devices, new_cap * sizeof(device_t*));
    if (!new_devices) return -1;
    g_registry.devices = new_devices;
    g_registry.capacity = new_cap;
    return 0;
}

int device_registry_add(device_t* dev) {
    if (!dev || !dev->name[0]) return -1;
    
    // Check for duplicates
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.devices[i] && 
            (strcmp(g_registry.devices[i]->path, dev->path) == 0 ||
             strcmp(g_registry.devices[i]->name, dev->name) == 0)) {
            return -1;
        }
    }
    
    if (g_registry.count >= g_registry.capacity) {
        if (registry_resize() != 0) return -1;
    }
    
    g_registry.devices[g_registry.count++] = dev;
    
    // Emit hotplug event
    if (g_registry.hotplug_cb) {
        hotplug_event_t event = {
            .type = HOTPLUG_EVENT_ADD,
            .timestamp = 0  // Would use RTC
        };
        strncpy(event.device_path, dev->path, sizeof(event.device_path) - 1);
        if (dev->driver) {
            strncpy(event.driver_name, dev->driver->name, sizeof(event.driver_name) - 1);
        }
        event.bus_data = dev->bus_data;
        g_registry.hotplug_cb(&event, g_registry.hotplug_arg);
    }
    
    return 0;
}

int device_registry_remove(const char* path) {
    if (!path) return -1;
    
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.devices[i] && strcmp(g_registry.devices[i]->path, path) == 0) {
            device_t* dev = g_registry.devices[i];
            
            // Emit hotplug event before removing
            if (g_registry.hotplug_cb) {
                hotplug_event_t event = {
                    .type = HOTPLUG_EVENT_REMOVE,
                    .timestamp = 0
                };
                strncpy(event.device_path, dev->path, sizeof(event.device_path) - 1);
                if (dev->driver) {
                    strncpy(event.driver_name, dev->driver->name, sizeof(event.driver_name) - 1);
                }
                event.bus_data = dev->bus_data;
                g_registry.hotplug_cb(&event, g_registry.hotplug_arg);
            }
            
            // Shift remaining devices
            for (size_t j = i; j < g_registry.count - 1; j++) {
                g_registry.devices[j] = g_registry.devices[j + 1];
            }
            g_registry.devices[g_registry.count - 1] = NULL;
            g_registry.count--;
            return 0;
        }
    }
    return -1;
}

device_t* device_registry_find(const char* path) {
    if (!path) return NULL;
    
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.devices[i] && strcmp(g_registry.devices[i]->path, path) == 0) {
            return g_registry.devices[i];
        }
    }
    return NULL;
}

device_t* device_registry_find_by_name(const char* name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < g_registry.count; i++) {
        if (g_registry.devices[i] && strcmp(g_registry.devices[i]->name, name) == 0) {
            return g_registry.devices[i];
        }
    }
    return NULL;
}

int device_registry_list(device_t*** list, size_t* count) {
    if (!list || !count) return -1;
    *list = g_registry.devices;
    *count = g_registry.count;
    return 0;
}

void device_registry_set_hotplug_callback(hotplug_callback_t cb, void* arg) {
    g_registry.hotplug_cb = cb;
    g_registry.hotplug_arg = arg;
}

void device_registry_emit_hotplug(hotplug_event_type_t type, const char* path,
                                   const char* driver_name, void* bus_data) {
    if (!g_registry.hotplug_cb || !path) return;
    
    hotplug_event_t event = {
        .type = type,
        .timestamp = 0
    };
    strncpy(event.device_path, path, sizeof(event.device_path) - 1);
    if (driver_name) {
        strncpy(event.driver_name, driver_name, sizeof(event.driver_name) - 1);
    }
    event.bus_data = bus_data;
    g_registry.hotplug_cb(&event, g_registry.hotplug_arg);
}

int i2c_scan_bus(hal_i2c_t* i2c, uint8_t* addrs, size_t max_addrs, size_t* found) {
    if (!i2c || !addrs || !found) return -1;
    
    *found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77 && *found < max_addrs; addr++) {
        uint8_t dummy;
        int ret = hal_i2c_read(i2c, addr, &dummy, 1);
        if (ret == 0) {
            addrs[(*found)++] = addr;
        }
    }
    return 0;
}

int spi_scan_bus(hal_spi_t* spi) {
    // SPI doesn't have standard device addresses like I2C
    // Would need chip-select scanning
    (void)spi;
    return 0;
}

int hotplug_scan_i2c(hal_i2c_t* i2c) {
    if (!i2c) return -1;
    
    uint8_t addrs[128];
    size_t found = 0;
    int ret = i2c_scan_bus(i2c, addrs, 128, &found);
    if (ret != 0) return ret;
    
    for (size_t i = 0; i < found; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/i2c/0x%02X", addrs[i]);
        
        // Check if device already registered
        if (device_registry_find(path)) continue;
        
        // Create device for this I2C address
        // In real implementation, would match to known drivers
        device_t* dev = calloc(1, sizeof(device_t));
        if (!dev) continue;
        
        snprintf(dev->name, sizeof(dev->name), "i2c-dev-0x%02X", addrs[i]);
        strncpy(dev->path, path, sizeof(dev->path) - 1);
        dev->bus_data = (void*)(uintptr_t)addrs[i];
        
        device_registry_add(dev);
    }
    
    return 0;
}

int hotplug_scan_spi(hal_spi_t* spi) {
    if (!spi) return -1;
    
    // SPI scanning would require chip-select iteration
    // For now, just a placeholder
    (void)spi;
    return 0;
}