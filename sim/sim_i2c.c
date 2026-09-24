#include "sim_i2c.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t addr;
    bool registered;
    sim_i2c_read_cb_t read_cb;
    sim_i2c_write_cb_t write_cb;
    void* arg;
} i2c_device_t;

static i2c_device_t g_devices[SIM_I2C_MAX_DEVICES];

int sim_i2c_init(void) {
    memset(g_devices, 0, sizeof(g_devices));
    return 0;
}

void sim_i2c_cleanup(void) {
}

int sim_i2c_register_device(uint8_t addr, sim_i2c_read_cb_t read_cb, sim_i2c_write_cb_t write_cb, void* arg) {
    for (int i = 0; i < SIM_I2C_MAX_DEVICES; i++) {
        if (!g_devices[i].registered) {
            g_devices[i].addr = addr;
            g_devices[i].read_cb = read_cb;
            g_devices[i].write_cb = write_cb;
            g_devices[i].arg = arg;
            g_devices[i].registered = true;
            return 0;
        }
    }
    return -1;
}

void sim_i2c_unregister_device(uint8_t addr) {
    for (int i = 0; i < SIM_I2C_MAX_DEVICES; i++) {
        if (g_devices[i].registered && g_devices[i].addr == addr) {
            g_devices[i].registered = false;
            return;
        }
    }
}

static i2c_device_t* find_device(uint8_t addr) {
    for (int i = 0; i < SIM_I2C_MAX_DEVICES; i++) {
        if (g_devices[i].registered && g_devices[i].addr == addr) {
            return &g_devices[i];
        }
    }
    return NULL;
}

int sim_i2c_write(uint8_t addr, const uint8_t* data, size_t len) {
    i2c_device_t* dev = find_device(addr);
    if (!dev || !dev->write_cb) return -1;
    return dev->write_cb(addr, 0, data, len, dev->arg);
}

int sim_i2c_read(uint8_t addr, uint8_t* data, size_t len) {
    i2c_device_t* dev = find_device(addr);
    if (!dev || !dev->read_cb) return -1;
    return dev->read_cb(addr, 0, data, len, dev->arg);
}

int sim_i2c_write_read(uint8_t addr, const uint8_t* write_data, size_t write_len, uint8_t* read_data, size_t read_len) {
    i2c_device_t* dev = find_device(addr);
    if (!dev) return -1;
    
    if (write_len > 0 && dev->write_cb) {
        int ret = dev->write_cb(addr, 0, write_data, write_len, dev->arg);
        if (ret < 0) return ret;
    }
    
    if (read_len > 0 && dev->read_cb) {
        return dev->read_cb(addr, 0, read_data, read_len, dev->arg);
    }
    
    return 0;
}