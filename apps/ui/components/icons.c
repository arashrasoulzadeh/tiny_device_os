#include "icons.h"

const app_icon_t app_icon_default = {{
    0x3FFC, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006,
    0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x6006, 0x3FFC,
}};

void app_icon_blit(int x, int y, const app_icon_t* icon, bool selected,
                   void (*set_pixel)(int px, int py, bool on, void* user), void* user, int scale) {
    const app_icon_t* src = icon ? icon : &app_icon_default;
    int r;
    int c;
    if (!set_pixel || scale <= 0) {
        return;
    }
    int size = APP_ICON_SIZE * scale;
    if (selected) {
        for (c = -1; c <= size; c++) {
            set_pixel(x + c, y - 1, true, user);
            set_pixel(x + c, y + size, true, user);
        }
        for (r = -1; r <= size; r++) {
            set_pixel(x - 1, y + r, true, user);
            set_pixel(x + size, y + r, true, user);
        }
    }
    for (r = 0; r < APP_ICON_SIZE; r++) {
        uint16_t bits = src->rows[r];
        for (c = 0; c < APP_ICON_SIZE; c++) {
            if (bits & (uint16_t)(0x8000u >> c)) {
                for (int sr = 0; sr < scale; sr++) {
                    for (int sc = 0; sc < scale; sc++) {
                        set_pixel(x + c * scale + sc, y + r * scale + sr, true, user);
                    }
                }
            }
        }
    }
}
