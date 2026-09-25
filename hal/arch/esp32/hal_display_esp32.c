#include "hal_display.h"
#include "hal_power.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>

#define DISPLAY_SPI_HOST SPI2_HOST

typedef struct hal_display {
    char path[32];
    hal_display_config_t config;
    spi_device_handle_t spi_handle;
    uint8_t* framebuffer;
    size_t fb_size;
    bool initialized;
    bool sleeping;
} hal_display_t;

static esp_err_t display_spi_write(hal_display_t* display, const uint8_t* data, size_t len) {
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    return spi_device_polling_transmit(display->spi_handle, &trans);
}

static void display_write_cmd(hal_display_t* display, uint8_t cmd) {
    gpio_set_level(display->config.pin_dc, 0);
    display_spi_write(display, &cmd, 1);
}

static void display_write_data(hal_display_t* display, const uint8_t* data, size_t len) {
    gpio_set_level(display->config.pin_dc, 1);
    display_spi_write(display, data, len);
}

hal_display_t* hal_display_open(const char* path, const hal_display_config_t* config) {
    hal_display_t* display = calloc(1, sizeof(hal_display_t));
    if (!display) return NULL;
    
    strncpy(display->path, path, sizeof(display->path) - 1);
    
    if (config) {
        display->config = *config;
    } else {
        display->config.width = 320;
        display->config.height = 240;
        display->config.bpp = 16;
        display->config.interface = HAL_DISPLAY_INTERFACE_SPI;
        display->config.spi_freq = 40000000;
        display->config.spi_mode = 0;
        display->config.color_format = HAL_DISPLAY_COLOR_RGB565;
        display->config.pin_dc = GPIO_NUM_2;
        display->config.pin_cs = GPIO_NUM_5;
        display->config.pin_rst = GPIO_NUM_4;
        display->config.pin_bl = GPIO_NUM_21;
    }
    
    display->fb_size = display->config.width * display->config.height * (display->config.bpp / 8);
    display->framebuffer = calloc(1, display->fb_size);
    display->sleeping = false;
    
    return display;
}

void hal_display_close(hal_display_t* display) {
    if (!display) return;
    if (display->initialized) {
        if (display->spi_handle) {
            spi_bus_remove_device(display->spi_handle);
        }
        spi_bus_free(DISPLAY_SPI_HOST);
    }
    free(display->framebuffer);
    free(display);
}

int hal_display_init(hal_display_t* display) {
    if (!display || display->initialized) return -1;
    
    // Configure pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << display->config.pin_dc) |
                       (1ULL << display->config.pin_cs) |
                       (1ULL << display->config.pin_rst) |
                       (1ULL << display->config.pin_bl),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Configure SPI
    spi_bus_config_t bus_config = {
        .mosi_io_num = GPIO_NUM_23,
        .miso_io_num = -1,
        .sclk_io_num = GPIO_NUM_18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return -1;
    
    spi_device_interface_config_t dev_config = {
        .mode = display->config.spi_mode,
        .clock_speed_hz = display->config.spi_freq,
        .spics_io_num = display->config.pin_cs,
        .queue_size = 4,
        .flags = display->config.cs_active_high ? SPI_DEVICE_POSITIVE_CS : 0,
    };
    
    err = spi_bus_add_device(SPI2_HOST, &dev_config, &display->spi_handle);
    if (err != ESP_OK) return -1;
    
    // Reset display
    gpio_set_level(display->config.pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(display->config.pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    
    // Basic init sequence (example for ILI9341/ST7789)
    display_write_cmd(display, 0x11); // Sleep out
    vTaskDelay(pdMS_TO_TICKS(120));
    display_write_cmd(display, 0x29); // Display on
    
    // Backlight on
    gpio_set_level(display->config.pin_bl, 1);
    
    display->initialized = true;
    return 0;
}

int hal_display_deinit(hal_display_t* display) {
    if (!display || !display->initialized) return -1;
    
    if (display->spi_handle) {
        spi_bus_remove_device(display->spi_handle);
        display->spi_handle = NULL;
    }
    spi_bus_free(SPI2_HOST);
    display->initialized = false;
    return 0;
}

int hal_display_draw_bitmap(hal_display_t* display, int16_t x, int16_t y,
                            uint16_t w, uint16_t h, const uint8_t* data) {
    if (!display || !display->initialized || !data) return -1;
    // Copy to framebuffer first
    size_t line_size = w * (display->config.bpp / 8);
    for (uint16_t row = 0; row < h; row++) {
        size_t fb_offset = ((y + row) * display->config.width + x) * (display->config.bpp / 8);
        memcpy(display->framebuffer + fb_offset, data + row * line_size, line_size);
    }
    // Then flush to display
    return hal_display_flush(display);
}

int hal_display_fill_rect(hal_display_t* display, int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint32_t color) {
    if (!display || !display->initialized) return -1;
    // Fill in framebuffer
    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < w; col++) {
            size_t offset = ((y + row) * display->config.width + x + col) * (display->config.bpp / 8);
            memcpy(display->framebuffer + offset, &color, display->config.bpp / 8);
        }
    }
    return hal_display_flush(display);
}

int hal_display_draw_pixel(hal_display_t* display, int16_t x, int16_t y, uint32_t color) {
    if (!display || !display->initialized) return -1;
    if (x < 0 || x >= display->config.width || y < 0 || y >= display->config.height) return -1;
    
    size_t offset = (y * display->config.width + x) * (display->config.bpp / 8);
    memcpy(display->framebuffer + offset, &color, display->config.bpp / 8);
    return hal_display_flush(display);
}

int hal_display_draw_line(hal_display_t* display, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint32_t color) {
    // Bresenham's line algorithm
    int16_t dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int16_t dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int16_t err = dx + dy, e2;
    
    while (1) {
        hal_display_draw_pixel(display, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
    return hal_display_flush(display);
}

int hal_display_draw_rect(hal_display_t* display, int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint32_t color) {
    hal_display_draw_line(display, x, y, x + w - 1, y, color);
    hal_display_draw_line(display, x, y + h - 1, x + w - 1, y + h - 1, color);
    hal_display_draw_line(display, x, y, x, y + h - 1, color);
    hal_display_draw_line(display, x + w - 1, y, x + w - 1, y + h - 1, color);
    return hal_display_flush(display);
}

int hal_display_set_rotation(hal_display_t* display, hal_display_rotation_t rotation) {
    if (!display) return -1;
    display->config.rotation = rotation;
    // Send rotation command to display
    return 0;
}

hal_display_rotation_t hal_display_get_rotation(const hal_display_t* display) {
    return display ? display->config.rotation : HAL_DISPLAY_ROTATION_0;
}

int hal_display_set_brightness(hal_display_t* display, uint8_t brightness) {
    if (!display) return -1;
    // PWM on backlight pin
    return 0;
}

uint8_t hal_display_get_brightness(const hal_display_t* display) {
    return display ? 255 : 0;
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

int hal_display_flush(hal_display_t* display) {
    if (!display || !display->initialized) return -1;
    
    // Send framebuffer to display in chunks
    // This is a simplified implementation
    return 0;
}

int hal_display_set_flush_cb(hal_display_t* display, hal_display_flush_cb_t cb, void* arg) {
    // Store callback
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