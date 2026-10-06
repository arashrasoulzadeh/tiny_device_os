#include "app_helper.h"

#include <stdio.h>

int app_fmt_clock(char* buf, size_t cap, int32_t seconds) {
    int n;
    if (!buf || cap == 0) {
        return -1;
    }
    if (seconds < 0) {
        seconds = 0;
    }
    n = snprintf(buf, cap, "%02ld:%02ld", (long)(seconds / 60), (long)(seconds % 60));
    if (n < 0 || (size_t)n >= cap) {
        return -1;
    }
    return n;
}

int app_fit_text_scale(int width_px, int glyph_cols, int max_scale) {
    int scale;
    if (max_scale < 1) {
        max_scale = 1;
    }
    if (glyph_cols < 1 || width_px < 1) {
        return 1;
    }
    scale = width_px / (glyph_cols * 6);
    if (scale > max_scale) {
        scale = max_scale;
    }
    if (scale < 1) {
        scale = 1;
    }
    return scale;
}

int app_center_in(int origin, int box, int item) {
    return origin + (box - item) / 2;
}

int app_bar_fill_px(int32_t value, int32_t total, int width_px) {
    int fill;
    if (width_px <= 0 || total <= 0 || value <= 0) {
        return 0;
    }
    if (value >= total) {
        return width_px;
    }
    fill = (int)(((int64_t)value * width_px) / total);
    if (fill < 0) {
        fill = 0;
    }
    if (fill > width_px) {
        fill = width_px;
    }
    return fill;
}

int app_gauge_fill_px(int32_t value, int32_t range, int half_width) {
    int32_t mag;
    int px;
    if (range <= 0 || half_width <= 0 || value == 0) {
        return 0;
    }
    mag = value < 0 ? -value : value;
    if (mag > range) {
        mag = range;
    }
    px = (int)(((int64_t)mag * half_width) / range);
    return value < 0 ? -px : px;
}

uint16_t app_level_color(int32_t value, int32_t mid, int32_t low) {
    if (value <= low) {
        return ARDUBOT_COLOR_DANGER;
    }
    if (value <= mid) {
        return ARDUBOT_COLOR_WARNING;
    }
    return ARDUBOT_COLOR_TEXT;
}

static void on_up(void* raw, void* user) {
    (void)raw;
    app_helper_emit((app_helper_t*)user, APP_EV_UP);
}

static void on_select(void* raw, void* user) {
    (void)raw;
    app_helper_emit((app_helper_t*)user, APP_EV_SELECT);
}

int app_helper_start(app_helper_t* app, void* real_app, const app_helper_desc_t* desc) {
    app_ui_config_t cfg;
    app_ui_key_def_t keys[6];
    if (!app || !desc) {
        return -1;
    }
    app->desc = desc;
    app->needs_draw = true;
    app_ui_config_ui(&cfg, desc->title ? desc->title : (desc->name ? desc->name : ""),
                     desc->help ? desc->help : "");
    if (app_ui_init(&app->ui, real_app, &cfg) != 0) {
        return -1;
    }
    keys[0] = (app_ui_key_def_t){SIM_KEY_UP, on_up, app};
    keys[1] = (app_ui_key_def_t){SIM_KEY_1, on_up, app};
    keys[2] = (app_ui_key_def_t){SIM_KEY_ENTER, on_select, app};
    keys[3] = (app_ui_key_def_t){SIM_KEY_2, on_select, app};
    keys[4] = (app_ui_key_def_t){SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL};
    keys[5] = (app_ui_key_def_t){0, NULL, NULL};
    if (app_ui_bind_keys(&app->ui, keys) < 0) {
        return -1;
    }
    return 0;
}

void app_helper_stop(app_helper_t* app) {
    if (!app) {
        return;
    }
    if (app->desc && app->desc->on_cleanup) {
        app->desc->on_cleanup(app);
    }
    app_ui_deinit(&app->ui);
}

void app_helper_invalidate(app_helper_t* app) {
    if (app) {
        app->needs_draw = true;
    }
}

void app_helper_emit(app_helper_t* app, app_helper_event_t ev) {
    if (!app || ev == 0) {
        return;
    }
    if (app->desc && app->desc->on_event) {
        app->desc->on_event(app, ev);
    }
    app_helper_invalidate(app);
}

void app_helper_frame(app_helper_t* app) {
    if (!app || !app->needs_draw) {
        return;
    }
    app->needs_draw = false;
    app_ui_begin_frame(&app->ui);
    if (app->desc && app->desc->on_draw) {
        app->desc->on_draw(app);
    }
    app_ui_end_frame(&app->ui);
}

void app_helper_text(app_helper_t* app, int x, int y, const char* text, uint16_t rgb565) {
    if (!app) {
        return;
    }
    app_ui_text_color(&app->ui, x, y, text, rgb565);
}

void app_helper_label(app_helper_t* app, int row, const char* text) {
    if (!app) {
        return;
    }
    app_ui_line(&app->ui, row, text);
}

void app_helper_number(app_helper_t* app, int y, const char* text, int scale, uint16_t rgb565) {
    if (!app) {
        return;
    }
    app_ui_big_text(&app->ui, y, text, scale, rgb565);
}

void app_helper_clock(app_helper_t* app, int y, int32_t seconds, int scale, uint16_t rgb565) {
    char buf[16];
    if (app_fmt_clock(buf, sizeof(buf), seconds) < 0) {
        return;
    }
    app_helper_number(app, y, buf, scale, rgb565);
}

void app_helper_bar(app_helper_t* app, int y, int h, int fill_w, uint16_t fill) {
    if (!app) {
        return;
    }
    app_ui_bar(&app->ui, y, h, fill_w, ARDUBOT_COLOR_TRACK, fill);
}

void app_helper_gauge(app_helper_t* app, int y, int h, int fill_px_signed, uint16_t fill) {
    if (!app) {
        return;
    }
    app_ui_bar_centered(&app->ui, y, h, fill_px_signed, ARDUBOT_COLOR_TRACK, fill, ARDUBOT_COLOR_TEXT);
}

void app_helper_panel(app_helper_t* app, int x, int y, int w, int h, const char* label, uint16_t bg) {
    if (!app) {
        return;
    }
    app_ui_panel(&app->ui, x, y, w, h, label, 2, bg, ARDUBOT_COLOR_TEXT);
}
