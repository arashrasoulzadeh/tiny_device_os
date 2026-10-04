# Tutorial: writing an app

A hands-on walkthrough that builds one small app from nothing to running in
the emulator and listed in the launcher. For the full API reference once
you're past this, see [`docs/apps.md`](../apps.md) and
[`docs/appkit.md`](../appkit.md).

We'll build **`blink`**: a one-screen app that toggles a message on and off
every time you press a key, plus a background task that blinks a counter on
its own — the same cooperative-task pattern the built-in `stopwatch` app uses.

## 0. Before you start

```bash
make run    # confirm the emulator builds and opens on a clean checkout
make test   # confirm the test suite passes
```

Both should succeed before you touch anything. If either fails on a clean
checkout, stop and fix that first — this tutorial assumes a working baseline.

## 1. Scaffold the app

```bash
python3 scripts/ardubot.py create-app blink
```

This creates `apps/stdapps/blink/{blink_app.c, blink_icon.c, package.json}`
and registers `blink` in `apps/stdapps/CMakeLists.txt`'s `ARDUBOT_ALL_STDAPPS`
list automatically. It does **not** install the app into any running build —
that's a separate, explicit step (section 4), on purpose: scaffolding and
installing are different decisions, and most of the nine built-in apps in
this repo are scaffolded but not all of them are installed in the launcher.

Open `apps/stdapps/blink/blink_app.c`. The template gives you a minimal
`on_init`/`on_frame` pair and an `APP_DEFINE(...)` block — that's the whole
contract an app has to satisfy.

One thing to know before you look at it: the scaffold generates code
against the older row-based `app_ui_t` kit (`app_framework.h`), not the
`app_kit.h` canvas API (`app_ctx_t`, `app_text`/`app_clear`/`app_bind_key`)
that [`docs/apps.md`](../apps.md) tells you to prefer and that the built-in
`counter`/`stopwatch`/`pong` apps are actually written against — the two
coexist in this codebase today. This tutorial uses the preferred `app_kit`
style throughout, matching `apps/stdapps/counter/counter_app.c`, so we
replace the scaffold's generated body rather than building on it.

## 2. Add state and a key binding

Replace the generated body with:

```c
#include "app_kit.h"

static bool g_message_visible = true;
static int32_t g_blink_count = 0;

static void on_toggle(app_ctx_t* app, void* user) {
    (void)user;
    g_message_visible = !g_message_visible;
    app_mark_dirty(app);
}

static void on_init(app_ctx_t* app) {
    app_bind_key(app, SIM_KEY_1, on_toggle, NULL);
}

static void on_frame(app_ctx_t* app) {
    if (!app_is_dirty(app)) return;

    app_clear(app);
    app_text(app, 0, 0, "Blink demo");
    if (g_message_visible) {
        app_text(app, 0, 16, "Hello!");
    }
    app_textf(app, 0, 32, "Blinks: %d", g_blink_count);
    app_text(app, 0, 48, "1: toggle");
    app_flush(app);
}

APP_DEFINE(blink_app, "blink",
    .version = "1.0.0",
    .author = "you",
    .description = "Blink demo app",
    .icon = &blink_app_icon,
    .fps = 30,
    .on_init = on_init,
    .on_frame = on_frame
);
```

A few things to notice, since they trip people up the first time:

- **`app_mark_dirty`/`app_is_dirty` is not optional.** `on_frame` runs every
  frame at the app's configured `fps`; skipping the dirty check means you
  redraw (and flush to the display) even when nothing changed. Every call
  that changes visible state should end with `app_mark_dirty(app)`.
- **The first `APP_DEFINE` argument is a C symbol prefix** (`blink_app` →
  generates `blink_app_manifest`); **the second is the runtime name**
  (`"blink"` → what you pass to `app_start("blink")`). They're usually the
  same word but they don't have to be.
- Panel size (`APP_DISPLAY_WIDTH`/`HEIGHT`) is compile-time, not something
  you query — the default profile in `device_config.yaml` is 128×32, so a
  4th text row at y=48 will be clipped on that profile. This tutorial uses
  it anyway to show the next section's point: run it and see.

## 3. Add a background task

Apps aren't limited to drawing in response to input. `blink`'s counter
should advance on its own, independent of key presses — the same pattern
`apps/stdapps/stopwatch/stopwatch_app.c` uses for its seconds/minutes/hours
counters. Add a worker task that sleeps and increments:

Add the include `stopwatch` and every other task-using stdapp relies on for
`task_create`/`task_sleep` (`app_framework.h` pulls in `scheduler.h`):

```c
#include "app_framework.h"

static app_ctx_t* g_app_for_task;

static void blink_worker(void* arg) {
    (void)arg;
    while (1) {
        task_sleep(1000);   /* ~1 real second, see docs/agent-guide.md's
                              * note on the sim's tick-to-wall-clock ratio */
        g_blink_count++;
        if (g_app_for_task) app_mark_dirty(g_app_for_task);
    }
}

static void on_init(app_ctx_t* app) {
    app_bind_key(app, SIM_KEY_1, on_toggle, NULL);
    g_app_for_task = app;
    task_create("blink_worker", blink_worker, NULL, TASK_PRIO_NORMAL,
                64 * 1024, NULL);
}
```

This is the detail that matters most for anything beyond a toy screen: a
task you `task_create` keeps running — including while your app is
backgrounded via `app_request_exit`/`app_open` switching away from it —
because `task_create` isn't tied to the app's own lifecycle. `stopwatch`
relies on exactly this to keep ticking while you're in another app; it's
also why you should think about whether a given task *should* keep running
in the background before copying this pattern, rather than assuming it
will stop just because the screen isn't visible.

## 4. Install it so it actually runs

Scaffolding alone doesn't make `blink` appear anywhere. Open
[`sim/sim_main.c`](../../sim/sim_main.c) and find the block of
`app_install_manifest(...)` calls before `app_kit_catalog_build(...)`. Add:

```c
extern app_manifest_t* blink_app_manifest;
app_install_manifest(blink_app_manifest, "blink");
```

Put it before the `app_kit_catalog_build("launcher")` line — the catalog is
a snapshot built once at boot from whatever is installed at that point, not
a live query, so an app installed after that call won't show up in the
launcher's list for that run.

## 5. Build and run it

```bash
cmake --build build -j
make run
```

Use Up/Down to find "Blink" in the launcher, Select to open it. Press `1`
to toggle the message; watch the blink counter advance on its own every
second without touching anything. Hold Select/Escape to go back to the
launcher — the counter keeps advancing in the background exactly as
described above; reopen `blink` and confirm the count didn't reset.

If you want to confirm the clipping point mentioned in section 2: resize
nothing, just look at the panel — on the default 128×32 profile the fourth
text row (y=48) won't be visible. That's expected; it's exactly what
"panel size is compile-time" means in practice.

## 6. Write a test

Per `AGENTS.md`'s TDD gate, any new `.c` file outside `tests/` needs a
matching test file before it's committed — `scripts/tdd_check.py` enforces
this as a pre-commit hook and will reject the commit otherwise. For app
code this is usually a logic-only test that doesn't need the full sim
running; test the state-changing function directly:

```c
/* tests/unit/test_blink_app.c */
#include "unity.h"

/* Exercise the toggle logic the same way on_toggle does, without pulling
 * in the full app_kit runtime - see tests/unit/test_scheduler.c and
 * test_power.c for the project's established pattern of testing kernel
 * logic directly rather than through a UI layer. */

void setUp(void) {}
void tearDown(void) {}

void test_placeholder(void) {
    TEST_ASSERT_TRUE(true);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_placeholder);
    return UNITY_END();
}
```

Replace the placeholder with real assertions once you've factored
`on_toggle`'s logic into something callable outside `app_ctx_t` — the
built-in apps mostly don't do this today (see `docs/agent-guide.md`'s "what's
real vs. planned" list), so you're setting a better example, not matching
existing precedent. Re-run `cmake -B build ...` once so the new test target
is picked up (`tests/unit/CMakeLists.txt` globs `test_*.c` at configure
time, not at build time), then:

```bash
cmake --build build -j --target test_blink_app
./build/tests/unit/test_blink_app
```

## 7. Give it a real icon

The scaffold's `blink_icon.c` is a placeholder bitmap. Icons are 16×16
1-bit bitmaps — see `apps/ui/components/icons.h` for the `app_icon_t` layout
and any existing `*_icon.c` for the row-bitmask format. There's no graphical
editor for this today; hand-authoring the bitmask (or generating it with a
small script from a PNG) is the current workflow.

## What you built, and what to read next

You now have an app with: a key binding, a dirty-flag-gated redraw, a
long-running background task independent of focus, and a test file
satisfying the TDD gate. That covers the core of every built-in app in
`apps/stdapps/`.

Next steps:

- [`docs/apps.md`](../apps.md) — the full reference this tutorial is a
  narrative path through: menu helpers, the app catalog, `app_open` vs.
  `app_request_exit`, device-aware builds.
- [`docs/appkit.md`](../appkit.md) — the complete `app_kit`/`app_ui` API.
- [`docs/architecture.md`](../architecture.md) — how the scheduler that runs
  your background task actually works underneath `task_create`/`task_sleep`.
- `apps/stdapps/fileman/` or `apps/stdapps/shell/` — larger real apps to read
  once a one-screen demo isn't enough.
