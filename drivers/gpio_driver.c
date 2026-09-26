#include "driver.h"
#include "hal_gpio.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct gpio_device_data {
    hal_gpio_t* hal_gpio;
    int pin_number;
};

static int gpio_probe(device_t* dev) {
    if (!dev || !dev->bus_data) return -1;
    
    struct gpio_device_data* data = calloc(1, sizeof(struct gpio_device_data));
    if (!data) return -1;
    
    data->pin_number = (int)(uintptr_t)dev->bus_data;
    
    char path[64];
    snprintf(path, sizeof(path), "/dev/gpio%d", data->pin_number);
    
    data->hal_gpio = hal_gpio_open(path, HAL_GPIO_MODE_INPUT);
    if (!data->hal_gpio) {
        free(data);
        return -1;
    }
    
    dev->private_data = data;
    return 0;
}

static int gpio_remove(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    
    struct gpio_device_data* data = (struct gpio_device_data*)dev->private_data;
    if (data->hal_gpio) {
        hal_gpio_close(data->hal_gpio);
    }
    free(data);
    dev->private_data = NULL;
    return 0;
}

static int gpio_open(device_t* dev, void** handle) {
    if (!dev || !dev->private_data) return -1;
    *handle = dev->private_data;
    return 0;
}

static int gpio_close(void* handle) {
    (void)handle;
    return 0;
}

static ssize_t gpio_read(void* handle, void* buf, size_t count) {
    if (!handle || !buf || count < 1) return -1;
    
    struct gpio_device_data* data = (struct gpio_device_data*)handle;
    bool level = hal_gpio_read(data->hal_gpio);
    *(bool*)buf = level;
    return 1;
}

static ssize_t gpio_write(void* handle, const void* buf, size_t count) {
    if (!handle || !buf || count < 1) return -1;
    
    struct gpio_device_data* data = (struct gpio_device_data*)handle;
    hal_gpio_write(data->hal_gpio, *(const bool*)buf);
    return 1;
}

static int gpio_ioctl(void* handle, uint32_t cmd, void* arg) {
    if (!handle) return -1;
    
    struct gpio_device_data* data = (struct gpio_device_data*)handle;
    
    switch (cmd) {
        case 0x01:  // GPIO_SET_MODE
            if (arg) {
                hal_gpio_mode_t mode = *(hal_gpio_mode_t*)arg;
                // Would need to reopen with new mode
                return 0;
            }
            break;
        case 0x02:  // GPIO_GET_MODE
            if (arg) {
                // Return current mode
                return 0;
            }
            break;
        case 0x03:  // GPIO_SET_IRQ
            if (arg) {
                // Would set interrupt
                return 0;
            }
            break;
        case 0x04:  // GPIO_GET_PIN
            if (arg) {
                *(int*)arg = data->pin_number;
                return 0;
            }
            break;
        case 0x05:  // GPIO_TOGGLE
            hal_gpio_toggle(data->hal_gpio);
            return 0;
    }
    return -1;
}

static int gpio_suspend(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct gpio_device_data* data = (struct gpio_device_data*)dev->private_data;
    return hal_gpio_suspend(data->hal_gpio);
}

static int gpio_resume(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct gpio_device_data* data = (struct gpio_device_data*)dev->private_data;
    return hal_gpio_resume(data->hal_gpio);
}

static const driver_ops_t gpio_ops = {
    .probe = gpio_probe,
    .remove = gpio_remove,
    .open = gpio_open,
    .close = gpio_close,
    .read = gpio_read,
    .write = gpio_write,
    .ioctl = gpio_ioctl,
    .suspend = gpio_suspend,
    .resume = gpio_resume,
};

static driver_t g_gpio_driver = {
    .name = "gpio",
    .type = DRIVER_TYPE_CHAR,
    .ops = &gpio_ops,
};

int gpio_driver_init(void) {
    return driver_register(&g_gpio_driver);
}

void gpio_driver_deinit(void) {
    driver_unregister("gpio");
}

int gpio_create_device(int pin_number, const char* name) {
    device_t* dev = calloc(1, sizeof(device_t));
    if (!dev) return -1;
    
    if (name) {
        strncpy(dev->name, name, DEVICE_NAME_MAX - 1);
    } else {
        snprintf(dev->name, sizeof(dev->name), "gpio%d", pin_number);
    }
    
    snprintf(dev->path, sizeof(dev->path), "/dev/gpio%d", pin_number);
    dev->driver = &g_gpio_driver;
    dev->bus_data = (void*)(uintptr_t)pin_number;
    
    return device_register(dev);
}