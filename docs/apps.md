# Writing apps

Prefer **`app_helper.h`** for a new screen. Full API and component map: [`docs/appkit.md`](appkit.md).
`APP_DEFINE` in `app_kit.h` remains when an app cannot be expressed with the helper fields below.

This page is the short path: minimal example, install, and which app boots.

## App helper

```c
#include "app_helper.h"

#include <stdio.h>

static int32_t g_count;

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    if (ev == APP_EV_UP) g_count++;
    if (ev == APP_EV_SELECT) g_count--;
}

static void on_draw(app_helper_t* app) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)g_count);
    app_helper_number(app, 0, buf, 4, ARDUBOT_COLOR_TEXT);
}

APP_HELPER(counter_app, "counter",
    .title = "COUNTER",
    .help = "Up:+  Sel:-",
    .icon = &counter_app_icon,
    .on_event = on_event,
    .on_draw = on_draw);
```

Up and the `1` key increment. Down, Left, and Right are bound the same way. Select and the `2` key decrement. Escape leaves. The screen redraws when an event arrives, when `app_helper_invalidate(app)` runs, or when `.live = true`. `.on_tick` runs every frame before that check. `.on_ready` runs after the UI and keys exist (start a worker there). `.game = true` drops the title and help bars. `.keys` replaces the default map with a file-scope `app_ui_key_def_t` array; a `NULL` user pointer is filled in with the helper. `app_fmt_clock`, `app_fit_text_scale`, `app_bar_fill_px`, `app_gauge_fill_px`, `app_level_color`, `app_helper_clock`, `app_helper_bar`, `app_helper_gauge`, `app_helper_panel`, `app_helper_labelf`, and `app_helper_center_text` cover the layout math apps used to copy.

## App state

Point `.state` at a plain struct and set `.state_size`. The helper loads that struct when the app starts and when it resumes, and stores it when the app quits (leaves for the launcher, or the task stops). `app_set_state()` / `app_get_state()` are the same store if you need to update it from a worker. The copy stays in RAM. A later storage backend can sit behind those two calls.

## Clock and padding

The image boots **info** when that app is compiled in, otherwise clock, otherwise pomodoro, otherwise the launcher. The clock reads the wall clock (`os_clock_now`), not uptime. Up adds an hour and Select adds a minute.

Every standard app's content box is inset by `ARDUBOT_UI_PADDING` on all four sides (16px when the panel is taller than 64). Games stay full-bleed.

A board with no battery RTC gets its clock from the clock service. `make usb` writes `ARDUBOT_CLOCK_UNIX` and `ARDUBOT_CLOCK_TZ_OFFSET_MIN` from your computer unless `device_config`'s `clock.unix` is set. On boot the service reads `/flash/clock.dat` (LittleFS on the chip, through `vfs`). A saved time wins. If the file is missing, the compile-time stamp is written there and used. Up and Select change the clock and save it; the service also rewrites the file about once a minute so a power cut keeps the last minute. Time spent fully off is not counted.

## Sensors

List them in the device config. Each entry is a map with `key`, `type` (`adc`), optional `path` (default `/dev/adc0`), and `refresh_ms` (default 1000):

```yaml
sensors:
  - key: temp
    type: adc
    path: /dev/adc0
    refresh_ms: 1000
```

`make usb` writes that list into `device_config.h`. Boot calls `sensor_service_load_builtin()`. An app reads a key with `app_helper_sensor("temp", &value)` (same as `sensor_get`). The service samples the hardware once per `refresh_ms` and returns the cached value until that interval has passed.

## Lower-level app

```c
#include "app_kit.h"

static int32_t g_count;

static void on_inc(app_ctx_t* app, void* user) {
    (void)user;
    g_count++;
    app_mark_dirty(app);
}

static void on_dec(app_ctx_t* app, void* user) {
    (void)user;
    g_count--;
    app_mark_dirty(app);
}

static void on_init(app_ctx_t* app) {
    app_bind_key(app, SIM_KEY_1, on_inc, NULL);
    app_bind_key(app, SIM_KEY_2, on_dec, NULL);
}

static void on_frame(app_ctx_t* app) {
    if (!app_is_dirty(app)) return;
    app_clear(app);
    app_text(app, 0, 0, "Counter");
    app_textf(app, 0, 16, "Count: %d", g_count);
    app_text(app, 0, 32, "1:+  2:-");
    app_flush(app);
}

APP_DEFINE(counter_app, "counter",
    .version = "2.0.0",
    .author = "ArdubotOS",
    .description = "Simple counter demo",
    .icon = &counter_app_icon,   /* apps provide their own 16×16 icon */
    .fps = 30,
    .on_init = on_init,
    .on_frame = on_frame
);
```

Define the bitmap in `counter_icon.c` (or beside the app):

```c
#include "icons.h"
const app_icon_t counter_app_icon = {{ /* 16 row bitmasks */ }};
```

- First argument: C symbol prefix → exports `counter_app_manifest`
- Second argument: install/start name → `"counter"` for `app_start("counter")`

Reference: `apps/stdapps/counter/counter_app.c`.

## What `APP_DEFINE` does

1. Fills an `app_desc_t` with defaults (version `1.0.0`, type user, 30 FPS,
   small stack/heap), then applies your overrides.
2. Generates `<symbol>_entry` → `app_kit_run`.
3. Exports `app_manifest_t* <symbol>_manifest` via a constructor for
   `app_install_manifest(...)`.

## Screen helpers

UI drawing lives in [`apps/ui/components/`](../apps/ui/components/)
(`canvas.h`, `menu.h`, `catalog.h`). `app_kit.h` re-exports them as an umbrella.

| Call | Purpose | Header |
|------|---------|--------|
| `app_mark_dirty` / `app_is_dirty` / `app_clear_dirty` | Skip redraw when nothing changed | `canvas.h` |
| `app_clear` / `app_text` / `app_textf` / `app_flush` | Framebuffer draw | `canvas.h` |
| `app_status_draw` | Top-right Wi-Fi, signal bars, and battery | `status.h` |

## Menu helpers

| Call | Purpose | Header |
|------|---------|--------|
| `app_menu_init` / `app_menu_add` | Build a vertical list | `menu.h` |
| `app_menu_move` | Up/down + dirty; pass key `user` through | `menu.h` |
| `APP_MENU_ONE_UP` / `APP_MENU_ONE_DOWN` | Deltas for `app_menu_move` | `menu.h` |
| `app_menu_selected` | Current item | `menu.h` |
| `app_menu_draw` | Title + rows + optional help (uses `APP_DISPLAY_HEIGHT`) | `menu.h` |
| `app_type_tag` | `"[SYS]"` / `"[USR]"` / … | `catalog.h` |

Panel size is compile-time: `APP_DISPLAY_WIDTH` / `APP_DISPLAY_HEIGHT`
([`display.h`](../apps/ui/components/display.h)), set by CMake
(`ARDUBOT_DISPLAY_WIDTH` / `ARDUBOT_DISPLAY_HEIGHT`, default **128×32** — same as
`device_config.yaml`).

## App catalog (boot)

After installing builtins, call once (`catalog.h`):

```c
app_kit_catalog_build("launcher");  /* excludes home app */
app_start("launcher");
```

The launcher loads that snapshot with `app_menu_load_catalog` — it does **not**
re-query `app_list` on every focus/Esc.

## Input

`app_bind_key(app, SIM_KEY_*, handler, user)` auto-assigns a unique GPIO pin,
maps the sim key while the app is foreground, and calls `handler(app, user)` on
press. Up to `APP_KIT_MAX_KEYS` (8) bindings per app. Background apps ignore
keys and do not flush the display.

## Switching apps

| Call | Purpose |
|------|---------|
| `app_open(from, "name")` | Start or **resume** another app; suspends the caller |
| `app_request_exit(app)` | Soft-leave: suspend current (non-home) app and resume launcher |

Leaving a non-home app keeps it alive in the background (timer apps keep
ticking). Esc on `info` / `counter` / `stopwatch` soft-leaves; the launcher
uses `app_open` on Enter (resumes if the app was suspended).

## Adding a builtin to the build

1. Create `apps/stdapps/my_app/my_app.c` with `APP_DEFINE(my_app, "my_app", ...)`.
2. Add `my_app/my_app.c` (and its include dir) to `apps/stdapps/CMakeLists.txt`.
3. Install it in `sim/sim_main.c` with `app_install_manifest(my_app_manifest, "my_app")`.

## Which app starts (main / home app)

The simulator picks the startup app in [`sim/sim_main.c`](../sim/sim_main.c):

```c
app_install_manifest(counter_app_manifest, "counter");
app_install_manifest(info_app_manifest, "info");
app_install_manifest(stopwatch_app_manifest, "stopwatch");
app_install_manifest(pong_app_manifest, "pong");
app_install_manifest(settings_app_manifest, "settings");
app_install_manifest(fileman_app_manifest, "fileman");
app_install_manifest(shell_app_manifest, "shell");
app_install_manifest(demo_app_manifest, "demo");
app_install_manifest(launcher_app_manifest, "launcher");
app_kit_catalog_build("launcher");  /* launch list, once */
app_start("launcher");              /* <-- main app */
```

Change the string passed to `app_start(...)` to boot a different app (e.g.
`app_start("counter")`). Install every builtin you want listed in the launcher
before `app_kit_catalog_build`, then start the home app.

The **stopwatch** app (`apps/stdapps/stopwatch/stopwatch_app.c`) demos cooperative
multithreading: one worker task per time unit (`sw_sec` sleeps 1s and ticks
seconds; `sw_min` / `sw_hour` self-suspend and resume on wrap). Keys: `1`
start/stop, `2` reset, Esc back.

## Lower-level APIs

`app_framework.h` still exposes display/button/timer/config helpers if you need
to go below the kit. Prefer the kit for new code.

## Logging

`APP_INFO`, `APP_WARN`, `APP_ERROR`, `APP_DEBUG` — available via `app_kit.h`.
