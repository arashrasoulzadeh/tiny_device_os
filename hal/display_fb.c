#include "display_fb.h"

#include <stdlib.h>

void display_fb_init(display_fb_t* fb, uint16_t* px, int16_t w, int16_t h) {
    if (!fb) {
        return;
    }
    fb->px = px;
    fb->w = w;
    fb->h = h;
    fb->dirty = false;
    fb->dirty_y0 = 0;
    fb->dirty_y1 = 0;
}

static void mark_rows(display_fb_t* fb, int16_t y0, int16_t y1) {
    if (y0 >= y1) {
        return;
    }
    if (!fb->dirty) {
        fb->dirty_y0 = y0;
        fb->dirty_y1 = y1;
        fb->dirty = true;
        return;
    }
    if (y0 < fb->dirty_y0) {
        fb->dirty_y0 = y0;
    }
    if (y1 > fb->dirty_y1) {
        fb->dirty_y1 = y1;
    }
}

void display_fb_draw_pixel(display_fb_t* fb, int16_t x, int16_t y, uint16_t rgb565) {
    if (!fb || !fb->px || x < 0 || y < 0 || x >= fb->w || y >= fb->h) {
        return;
    }
    fb->px[(int32_t)y * fb->w + x] = rgb565;
    mark_rows(fb, y, (int16_t)(y + 1));
}

void display_fb_fill_rect(display_fb_t* fb, int16_t x, int16_t y, int16_t w, int16_t h,
                          uint16_t rgb565) {
    int16_t x1;
    int16_t y1;
    int16_t row;
    int16_t col;

    if (!fb || !fb->px || w <= 0 || h <= 0) {
        return;
    }
    x1 = (int16_t)(x + w);
    y1 = (int16_t)(y + h);
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x1 > fb->w) {
        x1 = fb->w;
    }
    if (y1 > fb->h) {
        y1 = fb->h;
    }
    if (x >= x1 || y >= y1) {
        return;
    }
    for (row = y; row < y1; row++) {
        uint16_t* dst = fb->px + (int32_t)row * fb->w + x;
        for (col = x; col < x1; col++) {
            *dst++ = rgb565;
        }
    }
    mark_rows(fb, y, y1);
}

void display_fb_draw_line(display_fb_t* fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          uint16_t rgb565) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    if (!fb) {
        return;
    }
    for (;;) {
        int e2;
        display_fb_draw_pixel(fb, x0, y0, rgb565);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 = (int16_t)(x0 + sx);
        }
        if (e2 <= dx) {
            err += dx;
            y0 = (int16_t)(y0 + sy);
        }
    }
}

bool display_fb_dirty_rows(const display_fb_t* fb, int16_t* y, int16_t* rows) {
    if (!fb || !fb->dirty) {
        return false;
    }
    if (y) {
        *y = fb->dirty_y0;
    }
    if (rows) {
        *rows = (int16_t)(fb->dirty_y1 - fb->dirty_y0);
    }
    return true;
}

void display_fb_clear_dirty(display_fb_t* fb) {
    if (!fb) {
        return;
    }
    fb->dirty = false;
    fb->dirty_y0 = 0;
    fb->dirty_y1 = 0;
}
