#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_I2C_ADDR 0x3C

typedef enum {
    SSD1306_CMD_SET_CONTRAST = 0x81,
    SSD1306_CMD_DISPLAY_ALL_ON_RESUME = 0xA4,
    SSD1306_CMD_DISPLAY_ALL_ON = 0xA5,
    SSD1306_CMD_NORMAL_DISPLAY = 0xA6,
    SSD1306_CMD_INVERT_DISPLAY = 0xA7,
    SSD1306_CMD_DISPLAY_OFF = 0xAE,
    SSD1306_CMD_DISPLAY_ON = 0xAF,
    SSD1306_CMD_SET_DISPLAY_OFFSET = 0xD3,
    SSD1306_CMD_SET_COM_PINS = 0xDA,
    SSD1306_CMD_SET_VCOM_DETECT = 0xDB,
    SSD1306_CMD_SET_DISPLAY_CLOCK_DIV = 0xD5,
    SSD1306_CMD_SET_PRECHARGE = 0xD9,
    SSD1306_CMD_SET_MULTIPLEX = 0xA8,
    SSD1306_CMD_SET_LOW_COLUMN = 0x00,
    SSD1306_CMD_SET_HIGH_COLUMN = 0x10,
    SSD1306_CMD_SET_START_LINE = 0x40,
    SSD1306_CMD_MEMORY_MODE = 0x20,
    SSD1306_CMD_COLUMN_ADDR = 0x21,
    SSD1306_CMD_PAGE_ADDR = 0x22,
    SSD1306_CMD_COM_SCAN_INC = 0xC0,
    SSD1306_CMD_COM_SCAN_DEC = 0xC8,
    SSD1306_CMD_SEG_REMAP = 0xA0,
    SSD1306_CMD_CHARGE_PUMP = 0x8D
} ssd1306_cmd_t;

void ssd1306_model_register(void);

int ssd1306_model_write_cmd(uint8_t cmd);
int ssd1306_model_write_data(const uint8_t* data, size_t len);

void ssd1306_model_render(void);

#ifdef __cplusplus
}
#endif