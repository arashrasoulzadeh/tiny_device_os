#include "driver.h"
#include "hal_i2c.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct i2c_device_data {
    hal_i2c_t* hal_i2c;
    uint8_t addr;
};

static int i2c_probe(device_t* dev) {
    if (!dev || !dev->bus_data) return -1;
    
    struct i2c_device_data* data = calloc(1, sizeof(struct i2c_device_data));
    if (!data) return -1;
    
    data->addr = (uint8_t)(uintptr_t)dev->bus_data;
    
    // Get I2C bus from path or use default
    char bus_path[64];
    snprintf(bus_path, sizeof(bus_path), "/dev/i2c0");
    
    data->hal_i2c = hal_i2c_open(bus_path, HAL_I2C_SPEED_FAST);
    if (!data->hal_i2c) {
        free(data);
        return -1;
    }
    
    dev->private_data = data;
    return 0;
}

static int i2c_remove(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    
    struct i2c_device_data* data = (struct i2c_device_data*)dev->private_data;
    if (data->hal_i2c) {
        hal_i2c_close(data->hal_i2c);
    }
    free(data);
    dev->private_data = NULL;
    return 0;
}

static int i2c_open(device_t* dev, void** handle) {
    if (!dev || !dev->private_data) return -1;
    *handle = dev->private_data;
    return 0;
}

static int i2c_close(void* handle) {
    (void)handle;
    return 0;
}

static ssize_t i2c_read(void* handle, void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    struct i2c_device_data* data = (struct i2c_device_data*)handle;
    int ret = hal_i2c_read(data->hal_i2c, data->addr, buf, count);
    return ret == 0 ? (ssize_t)count : -1;
}

static ssize_t i2c_write(void* handle, const void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    struct i2c_device_data* data = (struct i2c_device_data*)handle;
    int ret = hal_i2c_write(data->hal_i2c, data->addr, buf, count);
    return ret == 0 ? (ssize_t)count : -1;
}

static int i2c_ioctl(void* handle, uint32_t cmd, void* arg) {
    if (!handle) return -1;
    
    struct i2c_device_data* data = (struct i2c_device_data*)handle;
    
    switch (cmd) {
        case 0x10:  // I2C_SET_ADDR
            if (arg) {
                data->addr = *(uint8_t*)arg;
                return 0;
            }
            break;
        case 0x11:  // I2C_GET_ADDR
            if (arg) {
                *(uint8_t*)arg = data->addr;
                return 0;
            }
            break;
        case 0x12:  // I2C_WRITE_READ
            if (arg) {
                // Would implement combined write-read
                return 0;
            }
            break;
        case 0x13:  // I2C_MEM_WRITE
            if (arg) {
                // Would implement memory write
                return 0;
            }
            break;
        case 0x14:  // I2C_MEM_READ
            if (arg) {
                // Would implement memory read
                return 0;
            }
            break;
        case 0x15:  // I2C_SCAN
            if (arg) {
                // Would implement bus scan
                return 0;
            }
            break;
        case 0x16:  // I2C_SET_SPEED
            if (arg) {
                hal_i2c_speed_t speed = *(hal_i2c_speed_t*)arg;
                // Would need to reopen
                return 0;
            }
            break;
    }
    return -1;
}

static int i2c_suspend(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct i2c_device_data* data = (struct i2c_device_data*)dev->private_data;
    return hal_i2c_suspend(data->hal_i2c);
}

static int i2c_resume(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct i2c_device_data* data = (struct i2c_device_data*)dev->private_data;
    return hal_i2c_resume(data->hal_i2c);
}

static const driver_ops_t i2c_ops = {
    .probe = i2c_probe,
    .remove = i2c_remove,
    .open = i2c_open,
    .close = i2c_close,
    .read = i2c_read,
    .write = i2c_write,
    .ioctl = i2c_ioctl,
    .suspend = i2c_suspend,
    .resume = i2c_resume,
};

static driver_t g_i2c_driver = {
    .name = "i2c",
    .type = DRIVER_TYPE_BUS,
    .ops = &i2c_ops,
};

int i2c_driver_init(void) {
    return driver_register(&g_i2c_driver);
}

void i2c_driver_deinit(void) {
    driver_unregister("i2c");
}

int i2c_create_device(uint8_t addr, const char* name) {
    device_t* dev = calloc(1, sizeof(device_t));
    if (!dev) return -1;
    
    if (name) {
        strncpy(dev->name, name, DEVICE_NAME_MAX - 1);
    } else {
        snprintf(dev->name, sizeof(dev->name), "i2c-dev-0x%02X", addr);
    }
    
    snprintf(dev->path, sizeof(dev->path), "/dev/i2c/0x%02X", addr);
    dev->driver = &g_i2c_driver;
    dev->bus_data = (void*)(uintptr_t)addr;
    
    return device_register(dev);
}