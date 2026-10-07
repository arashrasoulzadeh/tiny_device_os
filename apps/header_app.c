#include "header_app.h"
#include "fw/ui.h"
#include "os_time.h"
#include "status.h"
#include "theme.h"

#include <stdio.h>

static bool g_visible;

static uint16_t header_dim(uint16_t c, int shift) {
    uint16_t r = (uint16_t)((c >> 11) & 0x1F) >> shift;
    uint16_t g = (uint16_t)((c >> 5) & 0x3F) >> shift;
    uint16_t b = (uint16_t)(c & 0x1F) >> shift;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void header_app_set_visible(bool visible) {
    g_visible = visible;
}

bool header_app_visible(void) {
    return g_visible;
}

int app_header_height(void) {
    if (APP_DISPLAY_WIDTH < APP_HEADER_MIN_WIDTH || APP_DISPLAY_HEIGHT < APP_HEADER_MIN_HEIGHT) {
        return 0;
    }
    return APP_HEADER_BAND;
}

void app_header_draw(app_display_t* disp) {
    char clock_buf[8];
    uint32_t up_s;
    int w;
    int level;
    int i;
    uint16_t white = ARDUBOT_COLOR_TEXT;

    if (!disp || !disp->initialized || app_header_height() <= 0) {
        return;
    }

    w = APP_DISPLAY_WIDTH;
    /* Opaque band. Apps draw first; this call puts the header back. */
    app_display_fill_rect_color(disp, 0, 0, w, APP_HEADER_BAND, 0, ARDUBOT_COLOR_BG);
    app_display_draw_rect_color(disp, 6, 5, 26, 14, 2, white);
    app_display_fill_rect_color(disp, 32, 9, 3, 6, 0, white);
    level = (app_status_battery_percent() * 4) / 100;
    for (i = 0; i < 4; i++) {
        app_display_fill_rect_color(disp, 9 + i * 5, 8, 3, 8, 0,
                                    i < level ? white : header_dim(white, 3));
    }
    up_s = (uint32_t)(time_now_ms() / 1000);
    snprintf(clock_buf, sizeof(clock_buf), "%02u:%02u", (unsigned)((up_s / 60u) % 100u),
             (unsigned)(up_s % 60u));
    app_display_text_color(disp, (w - app_display_text_width(clock_buf, 2)) / 2, 5, clock_buf, 2,
                           white);
    app_display_hline_color(disp, 0, APP_HEADER_BAND - 1, w, header_dim(white, 4));
}

void header_app_composite(app_display_t* disp) {
    if (!g_visible) {
        return;
    }
    app_header_draw(disp);
}
