#pragma once

/**
 * App helper. One include and APP_HELPER(). Up / 1, Down, Left, Right,
 * and Select / 2 are already bound; Escape leaves the app. The frame is
 * drawn after an event, app_helper_invalidate(), or when .live is set.
 *
 * Pass .keys to replace that map (a file-scope array, one handler per
 * entry). .on_ready runs after the UI and keys exist. .on_tick runs
 * every frame before the redraw check. .game drops the title and help
 * bars. Two handlers on one key still go in .keys; the helper calls
 * each entry in order.
 */

#include "app_runtime.h"
#include "app_ui.h"
#include "theme.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_EV_UP = 1,
    APP_EV_DOWN,
    APP_EV_LEFT,
    APP_EV_RIGHT,
    APP_EV_SELECT,
} app_helper_event_t;

typedef struct app_helper app_helper_t;

typedef void (*app_helper_event_fn)(app_helper_t* app, app_helper_event_t ev);
typedef void (*app_helper_fn)(app_helper_t* app);

typedef struct {
    const char* name;
    const char* title;
    const char* help;
    const char* version;
    const char* author;
    const char* description;
    app_type_t type;
    uint32_t fps;
    const app_icon_t* icon;
    void* state;
    size_t state_size;
    bool game;
    bool live;
    const app_ui_key_def_t* keys;
    app_helper_event_fn on_event;
    app_helper_fn on_tick;
    app_helper_fn on_ready;
    app_helper_fn on_draw;
    app_helper_fn on_cleanup;
} app_helper_desc_t;

struct app_helper {
    app_ui_t ui;
    const app_helper_desc_t* desc;
    bool needs_draw;
    bool state_loaded;
};

/* "MM:SS" into buf. Negative seconds become 00:00. Returns the length
 * written, or -1 if buf cannot hold the text. */
int app_fmt_clock(char* buf, size_t cap, int32_t seconds);

/* Largest scale whose glyph columns fit in width_px. A 5x7 glyph cell is
 * 6px wide. Result is in [1, max_scale]. */
int app_fit_text_scale(int width_px, int glyph_cols, int max_scale);

/* origin + (box - item) / 2. */
int app_center_in(int origin, int box, int item);

/* value/total of width_px, clamped to [0, width_px]. */
int app_bar_fill_px(int32_t value, int32_t total, int width_px);

/* Signed fill from the center of a gauge. |value| is clamped to range. */
int app_gauge_fill_px(int32_t value, int32_t range, int half_width);

/* value <= low -> danger, value <= mid -> warning, otherwise body text. */
uint16_t app_level_color(int32_t value, int32_t mid, int32_t low);

int app_helper_start(app_helper_t* app, void* real_app, const app_helper_desc_t* desc);
void app_helper_stop(app_helper_t* app);
void app_helper_frame(app_helper_t* app);
void app_helper_emit(app_helper_t* app, app_helper_event_t ev);
void app_helper_invalidate(app_helper_t* app);

/* Copy this app's session to or from the runtime store. */
int app_helper_set_state(const app_helper_t* app, const void* data, size_t size);
int app_helper_get_state(const app_helper_t* app, void* data, size_t size);

/* True when start or resume loaded a snapshot into .state. */
static inline bool app_helper_has_state(const app_helper_t* app) {
    return app && app->state_loaded;
}

static inline int app_helper_content_w(const app_helper_t* app) {
    return app ? app->ui.ui.content_w : 0;
}

static inline int app_helper_content_h(const app_helper_t* app) {
    return app ? app->ui.ui.content_h : 0;
}

void app_helper_text(app_helper_t* app, int x, int y, const char* text, uint16_t rgb565);
void app_helper_center_text(app_helper_t* app, int x, int y, int w, int h, const char* text,
                            int scale, uint16_t rgb565);
void app_helper_label(app_helper_t* app, int row, const char* text);
void app_helper_labelf(app_helper_t* app, int row, const char* fmt, ...);
void app_helper_number(app_helper_t* app, int y, const char* text, int scale, uint16_t rgb565);
void app_helper_clock(app_helper_t* app, int y, int32_t seconds, int scale, uint16_t rgb565);
void app_helper_bar(app_helper_t* app, int y, int h, int fill_w, uint16_t fill);
void app_helper_gauge(app_helper_t* app, int y, int h, int fill_px_signed, uint16_t fill);
void app_helper_panel(app_helper_t* app, int x, int y, int w, int h, const char* label, uint16_t bg);

#define APP_HELPER(symbol, install_name, ...)                                                    \
    static app_helper_t symbol##_helper;                                                           \
    static const app_helper_desc_t symbol##_helper_desc = {                                        \
        .name = (install_name),                                                                \
        __VA_ARGS__};                                                                          \
    static void symbol##_helper_init(void* raw) {                                                \
        app_helper_start(&symbol##_helper, raw, &symbol##_helper_desc);                              \
    }                                                                                          \
    static void symbol##_helper_frame_fn(void* raw) {                                            \
        (void)raw;                                                                             \
        app_helper_frame(&symbol##_helper);                                                        \
    }                                                                                          \
    static void symbol##_helper_cleanup_fn(void* raw) {                                          \
        (void)raw;                                                                             \
        app_helper_stop(&symbol##_helper);                                                         \
    }                                                                                          \
    APP_DEFINE(symbol, install_name, .version = symbol##_helper_desc.version,                    \
               .author = symbol##_helper_desc.author,                                            \
               .description = symbol##_helper_desc.description, .type = symbol##_helper_desc.type, \
               .icon = symbol##_helper_desc.icon, .fps = symbol##_helper_desc.fps,                 \
               .on_init = symbol##_helper_init, .on_frame = symbol##_helper_frame_fn,              \
               .on_cleanup = symbol##_helper_cleanup_fn)

#ifdef __cplusplus
}
#endif
