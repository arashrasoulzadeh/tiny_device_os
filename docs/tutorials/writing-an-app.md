# Tutorial: writing an app

A hands-on walkthrough that builds one small app from nothing to running in
the emulator and listed in the launcher. For the reference once you're past
this, see [`docs/apps.md`](../apps.md) and [`docs/appkit.md`](../appkit.md).

We'll build **`blink`**: a one-screen app that toggles a message when you
press Select, plus a background task that counts on its own. That is the
same cooperative-task pattern the built-in `stopwatch` app uses.

## 0. Before you start

```bash
make run    # confirm the emulator builds and opens on a clean checkout
make test   # confirm the test suite passes
```

Both should succeed before you touch anything. If either fails on a clean
checkout, stop and fix that first.

## 1. Scaffold the app

```bash
python3 scripts/ardubot.py create-app blink
```

This creates `apps/stdapps/blink/{blink_app.c, blink_icon.c, app.json}` and
adds `blink` to `apps/stdapps/CMakeLists.txt`. Compiling the directory is
not the same as installing the app. Section 4 does that.

`app.json` holds the name, version, author, description, title, and help.
The compile copies them into the manifest. `APP_HELPER` does not repeat
them, and it uses `blink_app_icon` from `blink_icon.c` on its own. `.type`
defaults to tool. A tool sleeps until a key or `.every_ms`. `.fps` applies
only when `.live` or `.game` is set, and then it defaults to 30.

Open `blink_app.c`. The scaffold is one `APP_HELPER` line. A tool fills
`.on_view` (rows, a hero, a bar). `.on_draw` is the canvas path.

## 2. Toggle a message

Replace the generated body with:

```c
#include "app_framework.h"

static bool g_message_visible = true;

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    if (ev == APP_EV_SELECT) {
        g_message_visible = !g_message_visible;
    }
}

static void on_view(app_helper_t* app) {
    app_scene_row(app, 0, "%s", g_message_visible ? "Hello!" : "----");
}

APP_HELPER(blink_app, "blink", .on_event = on_event, .on_view = on_view)
```

A few things that trip people up:

- **Up, Down, Left, Right, Select, and Escape are already bound.** Select
  toggles the message. Escape leaves the app. You pass `.keys` only when
  that map is wrong.
- **The screen redraws after an event.** Call `app_helper_invalidate(app)`
  when something else changes the picture, such as a worker task.
- **The first `APP_HELPER` argument is a C symbol** (`blink_app` →
  `blink_app_manifest` and `blink_app_icon`). **The second is the runtime
  name** (`"blink"`, the `app.json` `"name"`, and the key `app_start` uses).
- **Rows, not pixel y.** `app_scene_row(app, 0, ...)` is the first content
  line. The simulator panel is 320×172. The NodeMCU profile is 128×32, so a
  long list that fits the simulator can clip on that board.
- **A tool does not poll.** Pass `.every_ms` when the picture changes on a
  timer. `.on_draw` stays for a game or an editor that places its own pixels.

Put the top-bar title and the bottom-bar help in `app.json` (`"title"` and
`"help"`). The scaffold already wrote `BLINK` and `Bk:back`.

## 3. Add a background task

The counter should advance even when nobody presses a key. `stopwatch` does
this with `task_create` from `.on_ready`. Add:

```c
#include "app_framework.h"

static app_helper_t* g_app;
static int32_t g_blink_count;
static volatile bool g_alive;

static void blink_worker(void* arg) {
    (void)arg;
    while (g_alive) {
        task_sleep(1000); /* ~1 real second in the simulator */
        g_blink_count++;
        if (g_app) {
            app_helper_invalidate(g_app);
        }
    }
}

static void on_ready(app_helper_t* app) {
    g_app = app;
    g_alive = true;
    task_create("blink", blink_worker, NULL, TASK_PRIO_NORMAL, 0, NULL);
}

static void on_view(app_helper_t* app) {
    app_scene_row(app, 0, "Blinks: %ld", (long)g_blink_count);
    app_scene_row(app, 2, "%s", g_message_visible ? "Hello!" : "----");
}

static void on_cleanup(app_helper_t* app) {
    (void)app;
    g_alive = false;
}

APP_HELPER(blink_app, "blink",
           .on_event = on_event, .on_ready = on_ready,
           .on_view = on_view, .on_cleanup = on_cleanup)
```

A one-second refresh that only updates this screen is `.every_ms = 1000`
and `.on_tick`, with no task. `task_create` is for work that must keep
running after Escape suspends the app. The worker keeps running after
Escape suspends the app, which is how stopwatch keeps ticking in the
background. `.on_cleanup` runs on a hard stop. Set `g_alive` false there so
the loop can finish. Decide whether a task should keep running before you
copy this pattern.

## 4. Install it so it shows up

`create-app` compiled the sources. The launcher only lists apps that
`stdapps_install()` installs. Open `apps/stdapps_register.c` and add this
inside `stdapps_install()`, next to the other apps:

```c
#ifdef ARDUBOT_APP_BLINK_ENABLED
    extern app_manifest_t* blink_app_manifest;
    if (install_manifest(blink_app_manifest, "blink") != 0) {
        return -1;
    }
#endif
```

The catalog is a snapshot built once at the end of `stdapps_install()`. An
app installed after `app_kit_catalog_build("launcher")` will not appear in
that run.

Boot does not start the launcher. `stdapps_start_name()` starts **sensors**
on a full image, otherwise info, clock, pomodoro, or the launcher. Escape from
blink returns to the launcher.

`settings`, `fileman`, `shell`, and `demo` are compiled into the simulator
and are not installed, so they are not in the launcher list. blink will be,
once the block above is in place.

## 5. Build and run it

```bash
cmake --build build -j
make run
```

The home screen is sensors. Escape to the launcher, move with Up/Down, and
Select blink. Select toggles the message. The count advances about once a
second. Escape goes back to the launcher; reopen blink and the worker is
still the one you started if the app was only suspended.

## 6. Write a test

`scripts/tdd_check.py` rejects a new `.c` file outside `tests/` unless a
matching test already contains `RUN_TEST` or `TEST_ASSERT`. Test the logic
without the display. `tests/unit/test_app_manifest.c` and
`tests/unit/test_app_helper.c` are the pattern for app identity and chrome:

```c
/* tests/unit/test_blink_app.c */
#include "unity.h"

static bool visible = true;

static void toggle(void) {
    visible = !visible;
}

void setUp(void) { visible = true; }
void tearDown(void) {}

void test_select_toggles_the_message(void) {
    toggle();
    TEST_ASSERT_FALSE(visible);
    toggle();
    TEST_ASSERT_TRUE(visible);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_select_toggles_the_message);
    return UNITY_END();
}
```

Re-run cmake once so the new `test_*.c` is picked up, then:

```bash
cmake -B build -DARDUBOT_BUILD_SIM=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j --target test_blink_app
./build/tests/unit/test_blink_app
```

A host build configured as `build/sim/Debug` uses that directory instead of
`build`.

## 7. Give it a real icon

`blink_icon.c` is a placeholder. Icons are 16×16 1-bit bitmaps. See
`apps/ui/components/icons.h` for `app_icon_t` and any `*_icon.c` for the row
masks. The symbol must be `blink_app_icon`, because `APP_HELPER(blink_app, ...)`
references that name. There is no icon editor; edit the masks by hand or
generate them from a PNG.

## What you built, and what to read next

You now have an app with a Select handler, row drawing, a background task,
an install block, and a test. That is the shape of the stdapps.

Next:

- [`docs/apps.md`](../apps.md) — `app.json` fields, boot order, sensors, clock.
- [`docs/appkit.md`](../appkit.md) — the framework parts and the lower-level
  `APP_DEFINE` path.
- [`docs/architecture.md`](../architecture.md) — the scheduler under
  `task_create` / `task_sleep`.
- `apps/stdapps/counter/counter_app.c` — the smallest real screen.
- `apps/stdapps/stopwatch/stopwatch_app.c` — workers that outlive the screen.
