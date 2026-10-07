#pragma once

/**
 * App helper. One include and APP_HELPER(). Up / 1, Down, Left, Right,
 * and Select / 2 are already bound; Escape leaves the app. The frame is
 * drawn after an event, app_helper_invalidate(), or when .live is set.
 *
 * Pass .keys to replace that map (a file-scope array, one handler per
 * entry). .on_load runs once at boot, before the app task exists, and is
 * where RAM-heavy setup goes. .on_ready runs after the UI and keys exist.
 * .on_view fills a scene; .on_draw is the canvas path. .every_ms wakes
 * .on_tick. A tool
 * sleeps until a key, a notification, or that interval. .live and .game
 * keep the frame clock. .game and .fullscreen drop the header_app band and
 * the title and help bars. Two handlers on one key still go in .keys;
 * the helper calls each entry in order.
 *
 * Title and help come from app.json when those fields are left out.
 * The launcher icon is <symbol>_icon (pomodoro_app uses pomodoro_app_icon).
 * .type defaults to APP_TYPE_TOOL. .fps applies only with .live or .game,
 * and then it defaults to 30.
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

typedef void (*app_helper_event_fn)(app_helper_t *app, app_helper_event_t ev);
typedef void (*app_helper_fn)(app_helper_t *app);

/* A tool screen fills these slots in on_view. The framework places them.
 * Set on_view or on_draw, not both. */
#define APP_SCENE_ROWS 8
#define APP_SCENE_TEXT 48

int app_scene_panel_w(int content_w);
int app_scene_hero_scale(int content_w, int glyph_cols, int max_scale,
                         bool beside_panel);
int app_scene_block_h(int hero_scale, bool has_meter);
int app_scene_row_y(int row, int row_h, int block_h);

typedef struct {
  char rows[APP_SCENE_ROWS][APP_SCENE_TEXT];
  int row_count;
  char hero[APP_SCENE_TEXT];
  bool hero_set;
  uint16_t hero_color;
  bool bar_set;
  int32_t bar_value;
  int32_t bar_total;
  uint16_t bar_color;
  bool gauge_set;
  int32_t gauge_value;
  int32_t gauge_range;
  uint16_t gauge_color;
  char panel[APP_SCENE_TEXT];
  bool panel_set;
  uint16_t panel_color;
} app_scene_t;

typedef struct {
  const char *name;
  const char *title;
  const char *help;
  app_type_t type;
  uint32_t fps;
  const app_icon_t *icon;
  void *state;
  size_t state_size;
  bool game;
  bool fullscreen;
  bool live;
  /* Wake on_tick on this period. 0 means on_tick runs on every frame
   * the run loop actually takes (a tool takes none until a key). */
  uint32_t every_ms;
  const app_ui_key_def_t *keys;
  app_helper_event_fn on_event;
  app_helper_fn on_tick;
  app_helper_fn on_ready;
  /* Once, at boot, before this app's task exists. No UI. */
  void (*on_load)(void);
  app_helper_fn on_view;
  app_helper_fn on_draw;
  app_helper_fn on_cleanup;
  /* POWER_DEMAND_UNSET (0) keeps the default. LOW is for timers and
   * sensors; HIGH is for a game that needs the top clock. */
  power_demand_t demand;
} app_helper_desc_t;

struct app_helper {
  app_ui_t ui;
  const app_helper_desc_t *desc;
  bool needs_draw;
  bool state_loaded;
  bool saw_frame;
  /* Scheduler second of the last paint, and the tick the last on_tick
   * was due. The header clock and the body share that second. */
  uint32_t painted_s;
  uint32_t last_tick_ms;
  app_scene_t scene;
};

/* "MM:SS" into buf. Negative seconds become 00:00. Returns the length
 * written, or -1 if buf cannot hold the text. */
int app_fmt_clock(char *buf, size_t cap, int32_t seconds);

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

int app_helper_start(app_helper_t *app, void *real_app,
                     const app_helper_desc_t *desc);
void app_helper_stop(app_helper_t *app);
void app_helper_frame(app_helper_t *app);
void app_helper_emit(app_helper_t *app, app_helper_event_t ev);
void app_helper_invalidate(app_helper_t *app);

/* Copy this app's session to or from the runtime store. */
int app_helper_set_state(const app_helper_t *app, const void *data,
                         size_t size);
int app_helper_get_state(const app_helper_t *app, void *data, size_t size);

/* True when start or resume loaded a snapshot into .state. */
static inline bool app_helper_has_state(const app_helper_t *app) {
  return app && app->state_loaded;
}

static inline int app_helper_content_w(const app_helper_t *app) {
  return app && app->ui.ctx ? app->ui.ctx->ui.content_w : 0;
}

static inline int app_helper_content_h(const app_helper_t *app) {
  return app && app->ui.ctx ? app->ui.ctx->ui.content_h : 0;
}

void app_helper_text(app_helper_t *app, int x, int y, const char *text,
                     uint16_t rgb565);
void app_helper_center_text(app_helper_t *app, int x, int y, int w, int h,
                            const char *text, int scale, uint16_t rgb565);
void app_helper_label(app_helper_t *app, int row, const char *text);
void app_helper_labelf(app_helper_t *app, int row, const char *fmt, ...);
void app_helper_number(app_helper_t *app, int y, const char *text, int scale,
                       uint16_t rgb565);
void app_helper_clock(app_helper_t *app, int y, int32_t seconds, int scale,
                      uint16_t rgb565);
void app_helper_bar(app_helper_t *app, int y, int h, int fill_w, uint16_t fill);
void app_helper_gauge(app_helper_t *app, int y, int h, int fill_px_signed,
                      uint16_t fill);
void app_helper_panel(app_helper_t *app, int x, int y, int w, int h,
                      const char *label, uint16_t bg);

void app_scene_row(app_helper_t *app, int row, const char *fmt, ...);
void app_scene_hero(app_helper_t *app, const char *text, uint16_t rgb565);
void app_scene_clock(app_helper_t *app, int32_t seconds, uint16_t rgb565);
void app_scene_bar(app_helper_t *app, int32_t value, int32_t total,
                   uint16_t rgb565);
void app_scene_gauge(app_helper_t *app, int32_t value, int32_t range,
                     uint16_t rgb565);
void app_scene_panel(app_helper_t *app, const char *label, uint16_t bg);

/* Latest sample for a sensor key from the device config. Same as sensor_get().
 */
int app_helper_sensor(const char *key, int32_t *value);

#define APP_HELPER(symbol, install_name, ...)                                  \
  extern const app_icon_t symbol##_icon;                                       \
  static app_helper_t symbol##_helper;                                         \
  static const app_helper_desc_t symbol##_helper_desc = {                      \
      .name = (install_name), __VA_ARGS__};                                    \
  static void symbol##_helper_init(void *raw) {                                \
    app_helper_start(&symbol##_helper, raw, &symbol##_helper_desc);            \
  }                                                                            \
  static void symbol##_helper_frame_fn(void *raw) {                            \
    (void)raw;                                                                 \
    app_helper_frame(&symbol##_helper);                                        \
  }                                                                            \
  static void symbol##_helper_cleanup_fn(void *raw) {                          \
    (void)raw;                                                                 \
    app_helper_stop(&symbol##_helper);                                         \
  }                                                                            \
  APP_DEFINE(                                                                  \
      symbol, install_name,                                                    \
      .type = symbol##_helper_desc.type != 0 ? symbol##_helper_desc.type       \
                                             : APP_TYPE_TOOL,                  \
      .icon = &symbol##_icon, .fps = symbol##_helper_desc.fps,                 \
      .on_load = symbol##_helper_desc.on_load,                                 \
      .on_init = symbol##_helper_init, .on_frame = symbol##_helper_frame_fn,   \
      .on_cleanup = symbol##_helper_cleanup_fn)

#ifdef __cplusplus
}
#endif
