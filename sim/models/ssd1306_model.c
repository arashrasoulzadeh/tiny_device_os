#include "ssd1306_model.h"
#include "sim_video.h"
#include "sim_i2c.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

static void ssd1306_unregister(void) {
    if (g_initialized) {
        sim_i2c_unregister_device(SSD1306_I2C_ADDR);
        g_initialized = false;
    }
}

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
    ssd1306_unregister();
    
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
    
    // Debug: draw a test pattern if buffer is empty (check if all zeros)
    static bool first_render = true;
    if (first_render) {
        bool all_zero = true;
        for (int i = 0; i < SSD1306_BUFFER_SIZE; i++) {
            if (g_ssd1306.buffer[i] != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            // Fill with a test pattern
            for (int y = 0; y < sim_h; y++) {
                for (int x = 0; x < sim_w; x++) {
                    pixels[y * sim_w + x] = (x + y) % 2 ? 0xFFFFFFFF : 0xFF000000;
                }
            }
            first_render = false;
            return;
        }
        first_render = false;
    }
    
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

void ssd1306_model_clear(void) {
    memset(g_ssd1306.buffer, 0, SSD1306_BUFFER_SIZE);
    g_ssd1306.page = 0;
    g_ssd1306.column = 0;
}

void ssd1306_model_draw_text(int x, int y, const char* text) {
    if (!text) return;
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) return;
    
    static const uint8_t font_5x7[95][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, // ' ' (32)
        {0x00, 0x00, 0x5F, 0x00, 0x00}, // '!' (33)
        {0x00, 0x07, 0x00, 0x07, 0x00}, // '"' (34)
        {0x14, 0x7F, 0x14, 0x7F, 0x14}, // '#' (35)
        {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // '$' (36)
        {0x23, 0x13, 0x08, 0x64, 0x62}, // '%' (37)
        {0x36, 0x49, 0x55, 0x22, 0x50}, // '&' (38)
        {0x00, 0x05, 0x03, 0x00, 0x00}, // ''' (39)
        {0x00, 0x1C, 0x22, 0x41, 0x00}, // '(' (40)
        {0x00, 0x41, 0x22, 0x1C, 0x00}, // ')' (41)
        {0x14, 0x08, 0x3E, 0x08, 0x14}, // '*' (42)
        {0x08, 0x08, 0x3E, 0x08, 0x08}, // '+' (43)
        {0x00, 0x50, 0x30, 0x00, 0x00}, // ',' (44)
        {0x08, 0x08, 0x08, 0x08, 0x08}, // '-' (45)
        {0x00, 0x60, 0x60, 0x00, 0x00}, // '.' (46)
        {0x20, 0x10, 0x08, 0x04, 0x02}, // '/' (47)
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0' (48)
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1' (49)
        {0x42, 0x61, 0x51, 0x49, 0x46}, // '2' (50)
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3' (51)
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4' (52)
        {0x27, 0x45, 0x45, 0x45, 0x39}, // '5' (53)
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6' (54)
        {0x01, 0x71, 0x09, 0x05, 0x03}, // '7' (55)
        {0x36, 0x49, 0x49, 0x49, 0x36}, // '8' (56)
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9' (57)
        {0x00, 0x36, 0x36, 0x00, 0x00}, // ':' (58)
        {0x00, 0x56, 0x36, 0x00, 0x00}, // ';' (59)
        {0x08, 0x14, 0x22, 0x41, 0x00}, // '<' (60)
        {0x14, 0x14, 0x14, 0x14, 0x14}, // '=' (61)
        {0x00, 0x41, 0x22, 0x14, 0x08}, // '>' (62)
        {0x02, 0x01, 0x51, 0x09, 0x06}, // '?' (63)
        {0x32, 0x49, 0x79, 0x41, 0x3E}, // '@' (64)
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 'A' (65)
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // 'B' (66)
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C' (67)
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 'D' (68)
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E' (69)
        {0x7F, 0x09, 0x09, 0x09, 0x01}, // 'F' (70)
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 'G' (71)
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 'H' (72)
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // 'I' (73)
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // 'J' (74)
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // 'K' (75)
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // 'L' (76)
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 'M' (77)
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 'N' (78)
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 'O' (79)
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // 'P' (80)
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 'Q' (81)
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // 'R' (82)
        {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S' (83)
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // 'T' (84)
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 'U' (85)
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 'V' (86)
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 'W' (87)
        {0x63, 0x14, 0x08, 0x14, 0x63}, // 'X' (88)
        {0x07, 0x08, 0x70, 0x08, 0x07}, // 'Y' (89)
        {0x61, 0x51, 0x49, 0x45, 0x43}, // 'Z' (90)
        {0x00, 0x7F, 0x41, 0x41, 0x00}, // '[' (91)
        {0x02, 0x04, 0x08, 0x10, 0x20}, // '\' (92)
        {0x00, 0x41, 0x41, 0x7F, 0x00}, // ']' (93)
        {0x04, 0x02, 0x01, 0x02, 0x04}, // '^' (94)
        {0x40, 0x40, 0x40, 0x40, 0x40}, // '_' (95)
        {0x00, 0x01, 0x02, 0x04, 0x00}, // '`' (96)
        {0x20, 0x54, 0x54, 0x54, 0x78}, // 'a' (97)
        {0x7F, 0x48, 0x44, 0x44, 0x38}, // 'b' (98)
        {0x38, 0x44, 0x44, 0x44, 0x20}, // 'c' (99)
        {0x38, 0x44, 0x44, 0x48, 0x7F}, // 'd' (100)
        {0x38, 0x54, 0x54, 0x54, 0x18}, // 'e' (101)
        {0x08, 0x7E, 0x09, 0x01, 0x02}, // 'f' (102)
        {0x0C, 0x52, 0x52, 0x52, 0x3E}, // 'g' (103)
        {0x7F, 0x08, 0x04, 0x04, 0x78}, // 'h' (104)
        {0x00, 0x44, 0x7D, 0x40, 0x00}, // 'i' (105)
        {0x20, 0x40, 0x44, 0x3D, 0x00}, // 'j' (106)
        {0x7F, 0x10, 0x28, 0x44, 0x00}, // 'k' (107)
        {0x00, 0x41, 0x7F, 0x40, 0x00}, // 'l' (108)
        {0x7C, 0x04, 0x18, 0x04, 0x78}, // 'm' (109)
        {0x7C, 0x08, 0x04, 0x04, 0x78}, // 'n' (110)
        {0x38, 0x44, 0x44, 0x44, 0x38}, // 'o' (111)
        {0x7C, 0x14, 0x14, 0x14, 0x08}, // 'p' (112)
        {0x08, 0x14, 0x14, 0x18, 0x7C}, // 'q' (113)
        {0x7C, 0x08, 0x04, 0x04, 0x08}, // 'r' (114)
        {0x48, 0x54, 0x54, 0x54, 0x20}, // 's' (115)
        {0x04, 0x3F, 0x44, 0x40, 0x20}, // 't' (116)
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 'u' (117)
        {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 'v' (118)
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 'w' (119)
        {0x44, 0x28, 0x10, 0x28, 0x44}, // 'x' (120)
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 'y' (121)
        {0x44, 0x64, 0x54, 0x4C, 0x44}, // 'z' (122)
        {0x00, 0x08, 0x36, 0x41, 0x00}, // '{' (123)
        {0x00, 0x00, 0x7F, 0x00, 0x00}, // '|' (124)
        {0x00, 0x41, 0x36, 0x08, 0x00}, // '}' (125)
        {0x10, 0x08, 0x08, 0x10, 0x08}, // '~' (125)
        {0x78, 0x46, 0x41, 0x46, 0x78}, // DEL (127)
    };
    
    int char_x = x;
    int char_y = y;
    
    for (const char* p = text; *p; p++) {
        if (*p < 32 || *p > 126) continue;
        
        const uint8_t* glyph = font_5x7[*p - 32];
        
        for (int col = 0; col < 5; col++) {
            if (char_x + col >= SSD1306_WIDTH) break;
            uint8_t column_data = glyph[col];
            
            for (int row = 0; row < 7; row++) {
                if (char_y + row >= SSD1306_HEIGHT) break;
                if (column_data & (1 << row)) {
                    int px = char_x + col;
                    int py = char_y + row;
                    if (px >= 0 && px < SSD1306_WIDTH && py >= 0 && py < SSD1306_HEIGHT) {
                        int page = py / 8;
                        int bit = py % 8;
                        size_t idx = page * SSD1306_WIDTH + px;
                        if (idx < SSD1306_BUFFER_SIZE) {
                            g_ssd1306.buffer[idx] |= (1 << bit);
                        }
                    }
                }
            }
        }
        
        char_x += 6; // 5 pixels + 1 pixel spacing
    }
}