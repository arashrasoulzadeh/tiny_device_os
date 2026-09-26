#include "driver.h"
#include "hal_spi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct spi_device_data {
    hal_spi_t* hal_spi;
    uint8_t cs_pin;
};

static int spi_probe(device_t* dev) {
    if (!dev || !dev->bus_data) return -1;
    
    struct spi_device_data* data = calloc(1, sizeof(struct spi_device_data));
    if (!data) return -1;
    
    data->cs_pin = (uint8_t)(uintptr_t)dev->bus_data;
    
    char bus_path[64];
    snprintf(bus_path, sizeof(bus_path), "/dev/spi0");
    
    hal_spi_config_t config = {
        .frequency = 1000000,
        .mode = HAL_SPI_MODE_0,
        .bit_order = HAL_SPI_BIT_ORDER_MSB,
        .bits_per_word = 8,
        .cs_active_high = false
    };
    
    data->hal_spi = hal_spi_open(bus_path, &config);
    if (!data->hal_spi) {
        free(data);
        return -1;
    }
    
    dev->private_data = data;
    return 0;
}

static int spi_remove(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    
    struct spi_device_data* data = (struct spi_device_data*)dev->private_data;
    if (data->hal_spi) {
        hal_spi_close(data->hal_spi);
    }
    free(data);
    dev->private_data = NULL;
    return 0;
}

static int spi_open(device_t* dev, void** handle) {
    if (!dev || !dev->private_data) return -1;
    *handle = dev->private_data;
    return 0;
}

static int spi_close(void* handle) {
    (void)handle;
    return 0;
}

static ssize_t spi_read(void* handle, void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    struct spi_device_data* data = (struct spi_device_data*)handle;
    int ret = hal_spi_read(data->hal_spi, buf, count);
    return ret == 0 ? (ssize_t)count : -1;
}

static ssize_t spi_write(void* handle, const void* buf, size_t count) {
    if (!handle || !buf) return -1;
    
    struct spi_device_data* data = (struct spi_device_data*)handle;
    int ret = hal_spi_write(data->hal_spi, buf, count);
    return ret == 0 ? (ssize_t)count : -1;
}

static int spi_ioctl(void* handle, uint32_t cmd, void* arg) {
    if (!handle) return -1;
    
    struct spi_device_data* data = (struct spi_device_data*)handle;
    
    switch (cmd) {
        case 0x20:  // SPI_TRANSFER
            if (arg) {
                // Would implement full duplex transfer
                return 0;
            }
            break;
        case 0x21:  // SPI_SET_CONFIG
            if (arg) {
                hal_spi_config_t* config = (hal_spi_config_t*)arg;
                return hal_spi_set_config(data->hal_spi, config);
            }
            break;
        case 0x22:  // SPI_GET_CONFIG
            if (arg) {
                return hal_spi_get_config(data->hal_spi, (hal_spi_config_t*)arg);
            }
            break;
        case 0x23:  // SPI_CS_ASSERT
            hal_spi_cs_assert(data->hal_spi);
            return 0;
        case 0x24:  // SPI_CS_DEASSERT
            hal_spi_cs_deassert(data->hal_spi);
            return 0;
        case 0x25:  // SPI_SET_CS
            if (arg) {
                data->cs_pin = *(uint8_t*)arg;
                return 0;
            }
            break;
    }
    return -1;
}

static int spi_suspend(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct spi_device_data* data = (struct spi_device_data*)dev->private_data;
    return hal_spi_suspend(data->hal_spi);
}

static int spi_resume(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct spi_device_data* data = (struct spi_device_data*)dev->private_data;
    return hal_spi_resume(data->hal_spi);
}

static const driver_ops_t spi_ops = {
    .probe = spi_probe,
    .remove = spi_remove,
    .open = spi_open,
    .close = spi_close,
    .read = spi_read,
    .write = spi_write,
    .ioctl = spi_ioctl,
    .suspend = spi_suspend,
    .resume = spi_resume,
};

static driver_t g_spi_driver = {
    .name = "spi",
    .type = DRIVER_TYPE_BUS,
    .ops = &spi_ops,
};

int spi_driver_init(void) {
    return driver_register(&g_spi_driver);
}

void spi_driver_deinit(void) {
    driver_unregister("spi");
}

int spi_create_device(uint8_t cs_pin, const char* name) {
    device_t* dev = calloc(1, sizeof(device_t));
    if (!dev) return -1;
    
    if (name) {
        strncpy(dev->name, name, DEVICE_NAME_MAX - 1);
    } else {
        snprintf(dev->name, sizeof(dev->name), "spi-dev-cs%d", cs_pin);
    }
    
    snprintf(dev->path, sizeof(dev->path), "/dev/spi%d", cs_pin);
    dev->driver = &g_spi_driver;
    dev->bus_data = (void*)(uintptr_t)cs_pin;
    
    return device_register(dev);
}