#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_DISPLAY_INTERFACE_SPI = 0,
    HAL_DISPLAY_INTERFACE_I2C,
    HAL_DISPLAY_INTERFACE_PARALLEL_8,
    HAL_DISPLAY_INTERFACE_PARALLEL_16,
    HAL_DISPLAY_INTERFACE_RGB
} hal_display_interface_t;

typedef enum {
    HAL_DISPLAY_ROTATION_0 = 0,
    HAL_DISPLAY_ROTATION_90 = 1,
    HAL_DISPLAY_ROTATION_180 = 2,
    HAL_DISPLAY_ROTATION_270 = 3
} hal_display_rotation_t;

typedef enum {
    HAL_DISPLAY_COLOR_MONO = 1,
    HAL_DISPLAY_COLOR_RGB565 = 16,
    HAL_DISPLAY_COLOR_RGB888 = 24,
    HAL_DISPLAY_COLOR_ARGB8888 = 32
} hal_display_color_format_t;

typedef struct hal_display hal_display_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
    uint8_t bpp;
    hal_display_interface_t interface;
    uint32_t spi_freq;
    uint8_t spi_mode;
    int8_t pin_cs;
    int8_t pin_dc;
    int8_t pin_rst;
    int8_t pin_bl;
    bool swap_bytes;
    hal_display_color_format_t color_format;
} hal_display_config_t;

typedef void (*hal_display_flush_cb_t)(hal_display_t* display, void* arg);

hal_display_t* hal_display_open(const char* path, const hal_display_config_t* config);
void hal_display_close(hal_display_t* display);

int hal_display_init(hal_display_t* display);
int hal_display_deinit(hal_display_t* display);

int hal_display_draw_bitmap(hal_display_t* display, int16_t x, int16_t y,
                            uint16_t w, uint16_t h, const uint8_t* data);
int hal_display_fill_rect(hal_display_t* display, int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint32_t color);
int hal_display_draw_pixel(hal_display_t* display, int16_t x, int16_t y, uint32_t color);
int hal_display_draw_line(hal_display_t* display, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint32_t color);
int hal_display_draw_rect(hal_display_t* display, int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint32_t color);

int hal_display_set_rotation(hal_display_t* display, hal_display_rotation_t rotation);
hal_display_rotation_t hal_display_get_rotation(const hal_display_t* display);

int hal_display_set_brightness(hal_display_t* display, uint8_t brightness);
uint8_t hal_display_get_brightness(const hal_display_t* display);

int hal_display_sleep(hal_display_t* display);
int hal_display_wake(hal_display_t* display);

int hal_display_set_flush_cb(hal_display_t* display, hal_display_flush_cb_t cb, void* arg);

void hal_display_get_size(const hal_display_t* display, uint16_t* width, uint16_t* height);
const char* hal_display_get_path(const hal_display_t* display);

#ifdef __cplusplus
}
#endif