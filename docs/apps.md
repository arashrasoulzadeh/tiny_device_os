# Writing apps

Prefer **`app_kit.h`** for new apps. It removes boilerplate (manifest, entry loop,
GPIO key wiring) so an app is mostly init + frame + key handlers.

## Minimal app

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
    .fps = 30,
    .on_init = on_init,
    .on_frame = on_frame
);
```

- First argument: C symbol prefix → exports `counter_app_manifest`
- Second argument: install/start name → `"counter"` for `app_start("counter")`

Reference: `apps/stdapps/counter_app.c`.

## What `APP_DEFINE` does

1. Fills an `app_desc_t` with defaults (version `1.0.0`, type user, 30 FPS,
   small stack/heap), then applies your overrides.
2. Generates `<symbol>_entry` → `app_kit_run`.
3. Exports `app_manifest_t* <symbol>_manifest` via a constructor for
   `app_install_manifest(...)`.

## Screen helpers

| Call | Purpose |
|------|---------|
| `app_mark_dirty` / `app_is_dirty` / `app_clear_dirty` | Skip redraw when nothing changed |
| `app_clear` | Clear framebuffer |
| `app_text` / `app_textf` | Draw text (printf-style) |
| `app_flush` | Present + clear dirty |

## Menu helpers

| Call | Purpose |
|------|---------|
| `app_menu_init` / `app_menu_add` | Build a vertical list |
| `app_menu_move` | Up/down + scroll into view |
| `app_menu_selected` | Current item |
| `app_menu_draw` | Title + rows + optional help line |
| `app_type_tag` | `"[SYS]"` / `"[USR]"` / … |

## App catalog (boot)

After installing builtins, call once:

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
| `app_open(from, "name")` | Start/focus another app; suspends the caller |
| `app_request_exit(app)` | Leave the current app (typical Esc/back) |

Leaving a non-home app resumes `"launcher"` and remaps its keys. Esc on
`info` / `counter` calls `app_request_exit`; the launcher uses `app_open` on
Enter.

## Adding a builtin to the build

1. Create `apps/stdapps/my_app.c` with `APP_DEFINE(my_app, "my_app", ...)`.
2. Add `my_app.c` to `apps/stdapps/CMakeLists.txt`.
3. Install it in `sim/sim_main.c` with `app_install_manifest(my_app_manifest, "my_app")`.

## Which app starts (main / home app)

The simulator picks the startup app in [`sim/sim_main.c`](../sim/sim_main.c):

```c
app_install_manifest(counter_app_manifest, "counter");
app_install_manifest(info_app_manifest, "info");
app_install_manifest(launcher_app_manifest, "launcher");
app_kit_catalog_build("launcher");  /* launch list, once */
app_start("launcher");              /* <-- main app */
```

Change the string passed to `app_start(...)` to boot a different app (e.g.
`app_start("counter")`). Install every builtin you want listed in the launcher
before `app_kit_catalog_build`, then start the home app.

## Lower-level APIs

`app_framework.h` still exposes display/button/timer/config helpers if you need
to go below the kit. Prefer the kit for new code.

## Logging

`APP_INFO`, `APP_WARN`, `APP_ERROR`, `APP_DEBUG` — available via `app_kit.h`.
