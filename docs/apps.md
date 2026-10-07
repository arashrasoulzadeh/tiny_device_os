# Writing apps

Prefer **`app_framework.h`** for a new screen. Full API and component map: [`docs/appkit.md`](appkit.md).
`APP_DEFINE` in `app_kit.h` remains when an app cannot be expressed with the helper fields below.

This page is the short path: minimal example, install, and which app boots.

## App helper

```c
#include "app_framework.h"

#include <stdio.h>

static int32_t g_count;

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    if (ev == APP_EV_UP) g_count++;
    if (ev == APP_EV_SELECT) g_count--;
}

static void on_view(app_helper_t* app) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)g_count);
    app_scene_hero(app, buf, ARDUBOT_COLOR_TEXT);
    app_scene_gauge(app, g_count, 20, ARDUBOT_COLOR_SUCCESS);
}

APP_HELPER(counter_app, "counter",
    .state = &g_count,
    .state_size = sizeof(g_count),
    .on_event = on_event,
    .on_view = on_view);
```

Name, version, author, description, title, and help come from `apps/stdapps/<name>/app.json`. Configure and the device build both turn that file into the manifest. `APP_HELPER` does not take those fields. The launcher icon is `<symbol>_icon` (`counter_app` uses `counter_app_icon`). `.type` defaults to `APP_TYPE_TOOL`. `.fps` applies only when `.live` or `.game` is set, and it defaults to 30.

Up and the `1` key increment. Down, Left, and Right are bound the same way. Select and the `2` key decrement. Escape leaves. A tool sleeps until a key, a notification, or `.every_ms`. `.on_view` fills a scene (rows, one hero number or clock, one bar or gauge, one side panel) and the framework places it. `.on_draw` is the canvas path for a game, a menu, or an editor. Set one of them. The screen redraws when an event arrives, when `app_helper_invalidate(app)` runs, when `.every_ms` elapses and `on_tick` invalidates, when `.live = true`, or once a second while the header band is showing so the body stays with the clock. `.on_load` runs once at boot, before the app task exists, and is where RAM-heavy setup goes. `.on_ready` runs after the UI and keys exist. `.demand` tells the power governor how much compute this screen wants (`POWER_DEMAND_LOW` for a timer or sensor list, `POWER_DEMAND_HIGH` for a game). Leave it unset for a normal interactive app. See [`docs/power.md`](power.md). `.game = true` or `.fullscreen = true` drops the header_app band and the title and help bars. `.keys` replaces the default map with a file-scope `app_ui_key_def_t` array; a `NULL` user pointer is filled in with the helper. Files and pins are `fw/io.h`, included only by a screen that opens them.

## App state

Point `.state` at a plain struct and set `.state_size`. The helper loads that struct when the app starts and when it resumes, and stores it when the app quits (leaves for the launcher, or the task stops). `app_set_state()` / `app_get_state()` are the same store if you need to update it from a worker. The copy stays in RAM. A later storage backend can sit behind those two calls.

## Clock and padding

Boot paints a splash with **ArdubotOS**, the last 8 characters of the git commit, a progress bar, and the name of the service or app being loaded. It mounts storage, starts the clock, notifications, and sensors, then calls each installed app's `on_load` once. The image then starts **sensors** when that app is compiled in, otherwise info, otherwise clock, otherwise pomodoro, otherwise the launcher. The clock reads the wall clock (`os_clock_now`), not uptime. Up adds an hour and Select adds a minute.

Every standard app's content box is inset by `ARDUBOT_UI_PADDING` on all four sides (16px when the panel is taller than 64). On a panel at least 280×150, that box also starts below the status band from `header_app.c`: battery on the left, uptime clock in the center, a separator on the last row of a 25px band. The same band is on the launcher. Flush paints it after the app, so it stays put. Games and `.fullscreen` apps stay full-bleed.

A board with no battery RTC gets its clock from the clock service. `make usb` writes `ARDUBOT_CLOCK_UNIX` and `ARDUBOT_CLOCK_TZ_OFFSET_MIN` from your computer unless `device_config`'s `clock.unix` is set. On boot the service reads `/flash/clock.dat` (LittleFS on the chip, through `vfs`). A saved time wins. If the file is missing, the compile-time stamp is written there and used. Up and Select change the clock and save it; the service also rewrites the file about once a minute so a power cut keeps the last minute. Time spent fully off is not counted.

## Sensors

List them in the device config. Each entry is a map with `key`, `type` (`temp` for the chip temperature sensor, `adc`, or `cpu` / `ram` / `power`), optional `path` for an ADC (default `/dev/adc0`), and `refresh_ms` (default 1000). The ESP32-C6-LCD board registers `temp`. Boot also registers `cpu` (busy percent), `ram` (heap used percent), and `pwr` (CPU clock in MHz). This board has no current shunt, so the power line is the clock, not milliamps. The host simulator has no clock reading and shows `pwr --`. The **sensors** app lists every registered key.

```yaml
sensors:
  - key: temp
    type: temp
    refresh_ms: 1000
```

`make usb` writes that list into `device_config.h`. Boot calls `sensor_service_load_builtin()`. An app reads a key with `app_helper_sensor("temp", &value)` (same as `sensor_get`). A `temp` sample is decidegrees Celsius (253 is 25.3 C). The service samples the hardware once per `refresh_ms` and returns the cached value until that interval has passed.

## Notifications

Notifications are an OS surface, not an app. Do not add one to `stdapps_install()` or the launcher, and do not give it a manifest. Boot calls `notify_service_start()` next to the clock service. An app or another service posts a card:

```c
notify_spec_t spec = {
    .title = "Saved",
    .body = "Clock updated",
    .extent = NOTIFY_EXTENT_BAND, /* or NOTIFY_EXTENT_FULL */
    .band_percent = 0,            /* 0 = top 40% of the panel */
    .duration_ms = 0,             /* 0 = 3 seconds after the show animation */
};
notify_post(&spec);
```

The card slides down over whatever the focused app just drew, holds, then slides back up. `NOTIFY_EXTENT_FULL` covers the panel. `NOTIFY_EXTENT_BAND` covers a top band; a band shorter than one text line is raised to that line. Any app-key press dismisses the card and is not delivered to the app underneath. The queue holds four cards, including the one on screen. A fifth `notify_post()` returns -1.

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

`APP_HELPER` is the path above. `APP_DEFINE` is the lower-level registration it expands to.

1. Fills an `app_desc_t` with defaults (type user, 30 FPS, small stack/heap).
   Version, author, and description come from `app.json` unless an override sets them.
2. Generates `<symbol>_entry` → `app_kit_run`.
3. Exports `app_manifest_t* <symbol>_manifest` via a constructor. `stdapps_install()`
   passes that pointer to `app_install_manifest(...)`.

`APP_HELPER` then changes two of those defaults: an omitted `.type` becomes
`APP_TYPE_TOOL`, and the launcher icon is `<symbol>_icon`. An omitted `.fps`
stays 30.

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
([`display.h`](../apps/ui/components/display.h)). The NodeMCU profile in
`device_config.yaml` is **128×32**. A simulator build forces **320×172**,
the ESP32-C6 LCD panel. Override either with `-DARDUBOT_DISPLAY_WIDTH` /
`-DARDUBOT_DISPLAY_HEIGHT`.

## App catalog (boot)

After `stdapps_install()` the catalog is already built. Boot then calls
`app_start(stdapps_start_name())`, which is **sensors** on a full image.
The launcher loads that snapshot with `app_menu_load_catalog` — it does not
re-query `app_list` on every focus.

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

`python3 scripts/ardubot.py create-app my_app` writes
`apps/stdapps/my_app/{my_app_app.c, my_app_icon.c, app.json}` and adds `my_app`
to `apps/stdapps/CMakeLists.txt`. That compiles it. To install it, add an
`ARDUBOT_APP_MY_APP_ENABLED` block inside `stdapps_install()` in
`apps/stdapps_register.c`, the same way `counter` is installed. Boot does not
read `sim/sim_main.c` for the app list: `stdapps_install()` installs the
compiled-in set, then `stdapps_start_name()` picks the home app.

## Which app starts

After install, [`sim/sim_main.c`](../sim/sim_main.c) starts whatever
`stdapps_start_name()` returns:

1. **sensors**, when that app is compiled in
2. otherwise **info**
3. otherwise **clock**
4. otherwise **pomodoro**
5. otherwise the **launcher**

Escape from a non-home app resumes the launcher (`APP_KIT_HOME_NAME`). The
launcher catalog is built once inside `stdapps_install()` and excludes the
launcher itself. `settings`, `fileman`, `shell`, and `demo` are compiled on
the simulator but are not installed, so they do not appear in that list.

The **stopwatch** app (`apps/stdapps/stopwatch/stopwatch_app.c`) demos cooperative
multithreading: one worker task per time unit (`sw_sec` sleeps 1s and ticks
seconds; `sw_min` / `sw_hour` self-suspend and resume on wrap). Up starts and
stops, Select resets, Escape goes back.

## Lower-level APIs

`app_framework.h` is the include for a new screen. `app_kit.h` remains when a
screen cannot be expressed with `APP_HELPER`.

## Logging

`APP_INFO`, `APP_WARN`, `APP_ERROR`, `APP_DEBUG` — from `app_framework.h`.
