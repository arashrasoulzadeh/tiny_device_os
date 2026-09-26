#include "driver.h"
#include "hal_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct display_device_data {
    hal_display_t* hal_display;
    uint8_t* framebuffer;
    size_t fb_size;
};

static int display_probe(device_t* dev) {
    if (!dev) return -1;
    
    struct display_device_data* data = calloc(1, sizeof(struct display_device_data));
    if (!data) return -1;
    
    hal_display_config_t config = {
        .width = 128,
        .height = 64,
        .rotation = 0,
        .bpp = 1,
        .interface = HAL_DISPLAY_INTERFACE_I2C,
        .spi_freq = 1000000,
        .spi_mode = 0,
        .pin_cs = -1,
        .pin_dc = -1,
        .pin_rst = -1,
        .pin_bl = -1,
        .swap_bytes = false,
        .color_format = HAL_DISPLAY_COLOR_MONO
    };
    
    data->hal_display = hal_display_open("/dev/display0", &config);
    if (!data->hal_display) {
        free(data);
        return -1;
    }
    
    if (hal_display_init(data->hal_display) != 0) {
        hal_display_close(data->hal_display);
        free(data);
        return -1;
    }
    
    uint16_t w, h;
    hal_display_get_size(data->hal_display, &w, &h);
    data->fb_size = (w * h * config.bpp + 7) / 8;
    data->framebuffer = calloc(1, data->fb_size);
    if (!data->framebuffer) {
        hal_display_deinit(data->hal_display);
        hal_display_close(data->hal_display);
        free(data);
        return -1;
    }
    
    dev->private_data = data;
    return 0;
}

static int display_remove(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    
    struct display_device_data* data = (struct display_device_data*)dev->private_data;
    if (data->hal_display) {
        hal_display_deinit(data->hal_display);
        hal_display_close(data->hal_display);
    }
    free(data->framebuffer);
    free(data);
    dev->private_data = NULL;
    return 0;
}

static int display_open(device_t* dev, void** handle) {
    if (!dev || !dev->private_data) return -1;
    *handle = dev->private_data;
    return 0;
}

static int display_close(void* handle) {
    (void)handle;
    return 0;
}

static ssize_t display_read(void* handle, void* buf, size_t count) {
    (void)handle; (void)buf; (void)count;
    return -1;
}

static ssize_t display_write(void* handle, const void* buf, size_t count) {
    (void)handle; (void)buf; (void)count;
    return -1;
}

static int display_ioctl(void* handle, uint32_t cmd, void* arg) {
    if (!handle) return -1;
    
    struct display_device_data* data = (struct display_device_data*)handle;
    
    switch (cmd) {
        case 0x50:  // DISPLAY_DRAW_BITMAP
            if (arg) {
                // arg would be a struct with x, y, w, h, data
                return 0;
            }
            break;
        case 0x51:  // DISPLAY_FILL_RECT
            if (arg) {
                // Would implement
                return 0;
            }
            break;
        case 0x52:  // DISPLAY_DRAW_PIXEL
            if (arg) {
                // Would implement
                return 0;
            }
            break;
        case 0x53:  // DISPLAY_SET_ROTATION
            if (arg && data->hal_display) {
                return hal_display_set_rotation(data->hal_display, *(hal_display_rotation_t*)arg);
            }
            break;
        case 0x54:  // DISPLAY_GET_ROTATION
            if (arg && data->hal_display) {
                *(hal_display_rotation_t*)arg = hal_display_get_rotation(data->hal_display);
                return 0;
            }
            break;
        case 0x55:  // DISPLAY_SET_BRIGHTNESS
            if (arg && data->hal_display) {
                return hal_display_set_brightness(data->hal_display, *(uint8_t*)arg);
            }
            break;
        case 0x56:  // DISPLAY_GET_BRIGHTNESS
            if (arg && data->hal_display) {
                *(uint8_t*)arg = hal_display_get_brightness(data->hal_display);
                return 0;
            }
            break;
        case 0x57:  // DISPLAY_SLEEP
            if (data->hal_display) {
                return hal_display_sleep(data->hal_display);
            }
            break;
        case 0x58:  // DISPLAY_WAKE
            if (data->hal_display) {
                return hal_display_wake(data->hal_display);
            }
            break;
        case 0x59:  // DISPLAY_GET_SIZE
            if (arg && data->hal_display) {
                uint16_t w, h;
                hal_display_get_size(data->hal_display, &w, &h);
                ((uint16_t*)arg)[0] = w;
                ((uint16_t*)arg)[1] = h;
                return 0;
            }
            break;
        case 0x5A:  // DISPLAY_FLUSH
            if (data->hal_display && data->framebuffer) {
                // Would flush framebuffer to display
                return 0;
            }
            break;
    }
    return -1;
}

static int display_suspend(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct display_device_data* data = (struct display_device_data*)dev->private_data;
    return hal_display_suspend(data->hal_display);
}

static int display_resume(device_t* dev) {
    if (!dev || !dev->private_data) return -1;
    struct display_device_data* data = (struct display_device_data*)dev->private_data;
    return hal_display_resume(data->hal_display);
}

static const driver_ops_t display_ops = {
    .probe = display_probe,
    .remove = display_remove,
    .open = display_open,
    .close = display_close,
    .read = display_read,
    .write = display_write,
    .ioctl = display_ioctl,
    .suspend = display_suspend,
    .resume = display_resume,
};

static driver_t g_display_driver = {
    .name = "display",
    .type = DRIVER_TYPE_DISPLAY,
    .ops = &display_ops,
};

int display_driver_init(void) {
    return driver_register(&g_display_driver);
}

void display_driver_deinit(void) {
    driver_unregister("display");
}

int display_create_device(const char* name) {
    device_t* dev = calloc(1, sizeof(device_t));
    if (!dev) return -1;
    
    if (name) {
        strncpy(dev->name, name, DEVICE_NAME_MAX - 1);
    } else {
        strncpy(dev->name, "display0", DEVICE_NAME_MAX - 1);
    }
    
    strncpy(dev->path, "/dev/display0", sizeof(dev->path) - 1);
    dev->driver = &g_display_driver;
    
    return device_register(dev);
}