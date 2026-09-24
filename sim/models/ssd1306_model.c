#include "ssd1306_model.h"
#include "sim_video.h"
#include "sim_i2c.h"
#include <stdlib.h>
#include <string.h>

#define SSD1306_PAGES 8
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_PAGES)

typedef struct {
    uint8_t buffer[SSD1306_BUFFER_SIZE];
    uint8_t page;
    uint8_t column;
    bool display_on;
    bool inverted;
    uint8_t contrast;
    uint8_t vcom;
    uint8_t multiplex;
    bool charge_pump;
    uint8_t memory_mode;
} ssd1306_t;

static ssd1306_t g_ssd1306 = {0};
static bool g_initialized = false;

static int ssd1306_i2c_write(uint8_t addr, uint16_t reg, const uint8_t* data, size_t len, void* arg) {
    (void)reg; (void)arg;
    if (addr != SSD1306_I2C_ADDR) return -1;
    if (len == 0) return 0;
    
    uint8_t control = data[0];
    if (control == 0x00) {
        for (size_t i = 1; i < len; i++) {
            ssd1306_model_write_cmd(data[i]);
        }
    } else if (control == 0x40) {
        ssd1306_model_write_data(data + 1, len - 1);
    }
    return 0;
}

static int ssd1306_i2c_read(uint8_t addr, uint16_t reg, uint8_t* data, size_t len, void* arg) {
    (void)addr; (void)reg; (void)data; (void)len; (void)arg;
    return 0;
}

void ssd1306_model_register(void) {
    if (g_initialized) return;
    
    memset(&g_ssd1306, 0, sizeof(g_ssd1306));
    g_ssd1306.display_on = true;
    g_ssd1306.contrast = 0x7F;
    g_ssd1306.vcom = 0x30;
    g_ssd1306.multiplex = 0x3F;
    g_ssd1306.charge_pump = true;
    g_ssd1306.memory_mode = 0x00;
    
    sim_i2c_register_device(SSD1306_I2C_ADDR, ssd1306_i2c_read, ssd1306_i2c_write, NULL);
    g_initialized = true;
}

int ssd1306_model_write_cmd(uint8_t cmd) {
    switch (cmd) {
        case SSD1306_CMD_DISPLAY_OFF:
            g_ssd1306.display_on = false;
            break;
        case SSD1306_CMD_DISPLAY_ON:
            g_ssd1306.display_on = true;
            break;
        case SSD1306_CMD_SET_CONTRAST:
            break;
        case SSD1306_CMD_NORMAL_DISPLAY:
            g_ssd1306.inverted = false;
            break;
        case SSD1306_CMD_INVERT_DISPLAY:
            g_ssd1306.inverted = true;
            break;
        case SSD1306_CMD_SET_MULTIPLEX:
            break;
        case SSD1306_CMD_SET_DISPLAY_OFFSET:
            break;
        case SSD1306_CMD_SET_DISPLAY_CLOCK_DIV:
            break;
        case SSD1306_CMD_SET_PRECHARGE:
            break;
        case SSD1306_CMD_SET_COM_PINS:
            break;
        case SSD1306_CMD_SET_VCOM_DETECT:
            break;
        case SSD1306_CMD_CHARGE_PUMP:
            break;
        case SSD1306_CMD_MEMORY_MODE:
            break;
        case SSD1306_CMD_COLUMN_ADDR:
            break;
        case SSD1306_CMD_PAGE_ADDR:
            break;
        case SSD1306_CMD_SET_START_LINE:
            break;
        case SSD1306_CMD_SEG_REMAP:
            break;
        case SSD1306_CMD_COM_SCAN_INC:
        case SSD1306_CMD_COM_SCAN_DEC:
            break;
        case SSD1306_CMD_SET_LOW_COLUMN:
            g_ssd1306.column = (g_ssd1306.column & 0xF0) | (cmd & 0x0F);
            break;
        case SSD1306_CMD_SET_HIGH_COLUMN:
            g_ssd1306.column = (g_ssd1306.column & 0x0F) | ((cmd & 0x0F) << 4);
            break;
        default:
            break;
    }
    return 0;
}

int ssd1306_model_write_data(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len && g_ssd1306.page < SSD1306_PAGES; i++) {
        size_t idx = g_ssd1306.page * SSD1306_WIDTH + g_ssd1306.column;
        if (idx < SSD1306_BUFFER_SIZE) {
            g_ssd1306.buffer[idx] = data[i];
            g_ssd1306.column++;
            if (g_ssd1306.column >= SSD1306_WIDTH) {
                g_ssd1306.column = 0;
                g_ssd1306.page++;
            }
        }
    }
    return 0;
}

void ssd1306_model_render(void) {
    if (!g_ssd1306.display_on) return;
    
    uint32_t* pixels = sim_video_get_pixels();
    int sim_w = sim_video_get_width();
    int sim_h = sim_video_get_height();
    
    if (!pixels) return;
    
    for (int y = 0; y < SSD1306_HEIGHT && y < sim_h; y++) {
        int page = y / 8;
        int bit = y % 8;
        
        for (int x = 0; x < SSD1306_WIDTH && x < sim_w; x++) {
            uint8_t val = g_ssd1306.buffer[page * SSD1306_WIDTH + x];
            bool pixel_on = (val >> bit) & 0x01;
            
            if (g_ssd1306.inverted) pixel_on = !pixel_on;
            
            uint32_t color = pixel_on ? 0xFFFFFFFF : 0xFF000000;
            pixels[y * sim_w + x] = color;
        }
    }
}