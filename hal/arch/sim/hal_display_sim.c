#include "hal_display.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct hal_display {
    char path[64];
    hal_display_config_t config;
    uint8_t* framebuffer;
    size_t fb_size;
    hal_display_flush_cb_t flush_cb;
    void* flush_arg;
    uint8_t brightness;
    bool sleeping;
};

hal_display_t* hal_display_open(const char* path, const hal_display_config_t* config) {
    hal_display_t* display = calloc(1, sizeof(hal_display_t));
    if (!display) return NULL;
    
    strncpy(display->path, path, sizeof(display->path) - 1);
    if (config) {
        display->config = *config;
    } else {
        display->config.width = 128;
        display->config.height = 64;
        display->config.rotation = 0;
        display->config.bpp = 1;
        display->config.interface = HAL_DISPLAY_INTERFACE_I2C;
        display->config.color_format = HAL_DISPLAY_COLOR_MONO;
    }
    
    display->fb_size = (display->config.width * display->config.height * display->config.bpp + 7) / 8;
    display->framebuffer = calloc(1, display->fb_size);
    display->brightness = 255;
    display->sleeping = false;
    
    return display;
}

void hal_display_close(hal_display_t* display) {
    if (display) {
        free(display->framebuffer);
        free(display);
    }
}

int hal_display_init(hal_display_t* display) {
    (void)display;
    return 0;
}

int hal_display_deinit(hal_display_t* display) {
    (void)display;
    return 0;
}

int hal_display_draw_bitmap(hal_display_t* display, int16_t x, int16_t y,
                            uint16_t w, uint16_t h, const uint8_t* data) {
    (void)display; (void)x; (void)y; (void)w; (void)h; (void)data;
    return 0;
}

int hal_display_fill_rect(hal_display_t* display, int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint32_t color) {
    (void)display; (void)x; (void)y; (void)w; (void)h; (void)color;
    return 0;
}

int hal_display_draw_pixel(hal_display_t* display, int16_t x, int16_t y, uint32_t color) {
    (void)display; (void)x; (void)y; (void)color;
    return 0;
}

int hal_display_draw_line(hal_display_t* display, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint32_t color) {
    (void)display; (void)x1; (void)y1; (void)x2; (void)y2; (void)color;
    return 0;
}

int hal_display_draw_rect(hal_display_t* display, int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint32_t color) {
    (void)display; (void)x; (void)y; (void)w; (void)h; (void)color;
    return 0;
}

int hal_display_set_rotation(hal_display_t* display, hal_display_rotation_t rotation) {
    if (!display) return -1;
    display->config.rotation = rotation;
    return 0;
}

hal_display_rotation_t hal_display_get_rotation(const hal_display_t* display) {
    return display ? display->config.rotation : HAL_DISPLAY_ROTATION_0;
}

int hal_display_set_brightness(hal_display_t* display, uint8_t brightness) {
    if (!display) return -1;
    display->brightness = brightness;
    return 0;
}

uint8_t hal_display_get_brightness(const hal_display_t* display) {
    return display ? display->brightness : 255;
}

int hal_display_sleep(hal_display_t* display) {
    if (!display) return -1;
    display->sleeping = true;
    return 0;
}

int hal_display_wake(hal_display_t* display) {
    if (!display) return -1;
    display->sleeping = false;
    return 0;
}

int hal_display_set_flush_cb(hal_display_t* display, hal_display_flush_cb_t cb, void* arg) {
    if (!display) return -1;
    display->flush_cb = cb;
    display->flush_arg = arg;
    return 0;
}

void hal_display_get_size(const hal_display_t* display, uint16_t* width, uint16_t* height) {
    if (display) {
        if (width) *width = display->config.width;
        if (height) *height = display->config.height;
    }
}

const char* hal_display_get_path(const hal_display_t* display) {
    return display ? display->path : NULL;
}

int hal_display_suspend(hal_display_t* display) {
    return hal_display_sleep(display);
}

int hal_display_resume(hal_display_t* display) {
    return hal_display_wake(display);
}