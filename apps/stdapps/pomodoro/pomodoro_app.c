#include "app_framework.h"
#include "app_kit.h"
#include "icons.h"
#include "theme.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern const app_icon_t pomodoro_app_icon;

/* Standard pomodoro cycle: 15 min of work, then 5 min of rest, looping
 * forever - not a one-shot 1-minute timer waiting to be dismissed. */
#define POMODORO_WORK_S (15 * 60)
#define POMODORO_REST_S (5 * 60)
#define POMODORO_LOW_S 10  /* <= this many seconds left in the phase: red */
#define POMODORO_MID_S 30  /* <= this many seconds left in the phase: orange */
/* How many flash toggles (1/sec) "done" shows before auto-advancing to
 * the next phase on its own - long enough to notice, short enough that
 * the cycle doesn't stall waiting for a press. */
#define POMODORO_DONE_FLASH_COUNT 6

typedef enum { POMODORO_PHASE_WORK, POMODORO_PHASE_REST } pomodoro_phase_t;

static pomodoro_phase_t g_phase;
static int32_t g_remaining_s;
static bool g_running;
static bool g_done;
static bool g_flash_on;
static int g_flash_count;
static volatile bool g_worker_alive;
static task_tcb_t* g_sec_task;

static app_ui_t g_ui;

static int32_t phase_total_s(pomodoro_phase_t phase) {
    return phase == POMODORO_PHASE_WORK ? POMODORO_WORK_S : POMODORO_REST_S;
}

static const char* phase_name(pomodoro_phase_t phase) {
    return phase == POMODORO_PHASE_WORK ? "WORK" : "REST";
}

static void pomodoro_start_phase(pomodoro_phase_t phase) {
    g_phase = phase;
    g_remaining_s = phase_total_s(phase);
    /* Auto-starts counting down, per the original ask ("on start...
     * rewinds from 1 minute to 0") - not left paused waiting for a
     * press. A reset mid-countdown (SELECT) starts the fresh countdown
     * running too, for the same reason. */
    g_running = true;
    g_done = false;
    g_flash_on = false;
    g_flash_count = 0;
}

static void pomodoro_restart(void) {
    pomodoro_start_phase(POMODORO_PHASE_WORK);
}

static void task_seconds(void* arg) {
    (void)arg;
    while (g_worker_alive) {
        task_sleep(1000);
        if (!g_worker_alive) {
            break;
        }
        bool changed = false;
        scheduler_lock();
        if (g_done) {
            /* Flash cadence while done: once per second is enough to read
             * as "flashing" without a second timer task. After
             * POMODORO_DONE_FLASH_COUNT toggles, auto-advance to the next
             * phase (work -> rest -> work -> ...) instead of sitting
             * there flashing forever waiting to be dismissed. */
            g_flash_on = !g_flash_on;
            g_flash_count++;
            if (g_flash_count >= POMODORO_DONE_FLASH_COUNT) {
                pomodoro_start_phase(g_phase == POMODORO_PHASE_WORK ? POMODORO_PHASE_REST
                                                                     : POMODORO_PHASE_WORK);
            }
            changed = true;
        } else if (g_running) {
            g_remaining_s--;
            if (g_remaining_s <= 0) {
                g_remaining_s = 0;
                g_running = false;
                g_done = true;
                g_flash_on = true;
                g_flash_count = 0;
            }
            changed = true;
        }
        scheduler_unlock();
        if (changed) {
            app_mark_dirty(NULL);
        }
    }
}

static void on_toggle_or_dismiss(void* app, void* user) {
    (void)app;
    (void)user;
    scheduler_lock();
    if (g_done) {
        /* Don't wait for the auto-advance flash countdown - skip
         * straight to the next phase. */
        pomodoro_start_phase(g_phase == POMODORO_PHASE_WORK ? POMODORO_PHASE_REST
                                                             : POMODORO_PHASE_WORK);
    } else {
        g_running = !g_running;
    }
    scheduler_unlock();
    app_mark_dirty(NULL);
    APP_INFO("pomodoro %s", g_done ? "advanced" : (g_running ? "running" : "paused"));
}

static void on_reset(void* app, void* user) {
    (void)app;
    (void)user;
    scheduler_lock();
    pomodoro_restart();
    scheduler_unlock();
    app_mark_dirty(NULL);
    APP_INFO("pomodoro reset");
}

static void on_init(void* app) {
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "POMODORO", "Up:start/pause Sel:reset");
    app_ui_init(&g_ui, app, &cfg);

    app_ui_bind_keys(&g_ui, (app_ui_key_def_t[]){
        {SIM_KEY_1, on_toggle_or_dismiss, NULL},
        {SIM_KEY_2, on_reset, NULL},
        {SIM_KEY_UP, on_toggle_or_dismiss, NULL},
        {SIM_KEY_ENTER, on_reset, NULL},
        {SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key, NULL},
        {0, NULL, NULL},
    });

    g_worker_alive = true;
    pomodoro_restart();

    if (task_create("pomo_sec", task_seconds, NULL, TASK_PRIO_NORMAL, 0, &g_sec_task) != 0) {
        APP_ERROR("Failed to create pomodoro worker task");
        g_worker_alive = false;
        return;
    }

    /* Draw the title/help bar chrome exactly once here - on_frame()'s
     * once-a-second redraw only repaints the content area below them
     * (see its own comment), so they'd otherwise never appear at all. */
    app_ui_begin_frame(&g_ui);
    app_ui_end_frame(&g_ui);

    APP_INFO("Pomodoro ready");
}

static void on_frame(void* app) {
    (void)app;
    int32_t remaining;
    bool running;
    bool done;
    bool flash_on;
    static int32_t last_remaining = INT32_MIN;
    static bool last_running = false;
    static bool last_done = false;
    static bool last_flash_on = false;
    static bool have_rendered = false;

    scheduler_lock();
    remaining = g_remaining_s;
    running = g_running;
    done = g_done;
    flash_on = g_flash_on;
    scheduler_unlock();

    /* task_seconds() only ever changes this state once a second, but
     * on_frame() runs at this app's full 30fps - redrawing (full clear +
     * redraw) on every one of those calls regardless was visible as a
     * flash on a real panel with no double buffer (confirmed on
     * hardware, RISCV_TODO.md Phase 4/5; same root cause info_app.c hit
     * and fixed). Skip the redraw entirely when nothing actually
     * changed since the last one. */
    if (have_rendered && remaining == last_remaining && running == last_running &&
        done == last_done && flash_on == last_flash_on) {
        return;
    }
    have_rendered = true;
    last_remaining = remaining;
    last_running = running;
    last_done = done;
    last_flash_on = flash_on;

    if (done) {
        /* Full-screen flash between the done accent color and black,
         * alternating once/second via task_seconds() above - this one
         * genuinely wants to flash the whole screen (including over the
         * title/help bars), so it still goes through begin/end_frame().
         * Names the phase that JUST finished and what's coming next -
         * phase has already flipped to the next one by the time g_done
         * clears, so show the one still ending (g_phase at this exact
         * instant is still the one that ran down to 0). */
        app_ui_begin_frame(&g_ui);
        const uint16_t flash_bg = flash_on ? ARDUBOT_COLOR_DANGER : ARDUBOT_COLOR_BG;
        app_ui_rect_color(&g_ui, 0, 0, g_ui.ui.content_w, g_ui.ui.content_h, 0, flash_bg);
        {
            char msg[24];
            snprintf(msg, sizeof(msg), "%s DONE!", phase_name(g_phase));
            int w = app_display_text_width(msg, 2);
            app_ui_text_color(&g_ui, g_ui.ui.content_x + (g_ui.ui.content_w - w) / 2,
                              g_ui.ui.content_h / 2 - 16, msg, ARDUBOT_COLOR_TEXT);
        }
        {
            char next[24];
            snprintf(next, sizeof(next), "Next: %s",
                     phase_name(g_phase == POMODORO_PHASE_WORK ? POMODORO_PHASE_REST
                                                                : POMODORO_PHASE_WORK));
            int w = app_display_text_width(next, 1);
            app_display_text_color(&g_ui.ctx.display,
                                   g_ui.ui.content_x + (g_ui.ui.content_w - w) / 2,
                                   g_ui.ui.content_y + g_ui.ui.content_h / 2 + 8, next, 1,
                                   ARDUBOT_COLOR_TEXT_MUTED);
        }
        app_ui_end_frame(&g_ui);
    } else {
        /* Two-zone layout (status block on the left, big countdown +
         * caption on the right, thin progress bar along the bottom) -
         * modeled after a real pomodoro timer's own display rather than
         * the old single-column stack, per explicit design feedback on
         * hardware. WORK is the demanding phase (warmer accent); REST
         * the easy one (cooler accent) - distinct from the separate
         * low-time warning colors used within either phase. */
        const uint16_t phase_color = g_phase == POMODORO_PHASE_WORK ? ARDUBOT_COLOR_ACCENT_WARM
                                                                     : ARDUBOT_COLOR_ACCENT_COOL;
        const int32_t phase_total = phase_total_s(g_phase);
        char line[8];
        uint16_t num_color = remaining <= POMODORO_LOW_S  ? ARDUBOT_COLOR_DANGER
                             : remaining <= POMODORO_MID_S ? ARDUBOT_COLOR_WARNING
                                                            : ARDUBOT_COLOR_TEXT;
        snprintf(line, sizeof(line), "%02ld:%02ld", (long)(remaining / 60), (long)(remaining % 60));

        const int bar_h = 8;
        /* app_ui_rect_color()'s own bounds check compares (y + content_y
         * + h) against content_h, not (content_y + content_h) - i.e. the
         * real usable height for a y=0 rect is (content_h - content_y),
         * not content_h itself. A box_h right up against content_h would
         * silently fail that check and never draw at all. */
        const int box_h = (g_ui.ui.content_h - g_ui.ui.content_y) - bar_h - 6;
        const int left_w = (g_ui.ui.content_w * 2) / 5;
        const int right_x = g_ui.ui.content_x + left_w;
        const int right_w = g_ui.ui.content_w - left_w;

        /* Left status block: a reusable colored panel (apps/app_ui.c)
         * with the phase name centered in it, same idea as a real
         * timer's "BUSY"/"FREE" panel. */
        app_ui_panel(&g_ui, 0, 0, left_w, box_h, phase_name(g_phase), 2, phase_color,
                    ARDUBOT_COLOR_TEXT);

        /* Right side: big MM:SS over a small caption - app_ui_text_color()
         * always uses this app's fixed text_scale (2), too small to read
         * as the headline element, so this draws directly at a larger
         * scale via app_display_text_color() (content_x/content_y
         * offsets added manually - app_ui_text_color() normally does
         * that, but it doesn't take a scale argument). The caption slot
         * doubles as a "PAUSED" indicator instead of a separate status
         * line - this layout has no spare row for one. */
        {
            const int num_scale = 4;
            const int num_w = app_display_text_width(line, num_scale);
            const int num_h = 7 * num_scale;
            const int caption_scale = 1;
            const char* caption = running ? "LEFT" : "PAUSED";
            const int caption_w = app_display_text_width(caption, caption_scale);
            const int block_h = num_h + 4 + 7 * caption_scale;
            const int num_y = (box_h - block_h) / 2;
            const int caption_y = num_y + num_h + 4;

            app_ui_rect_color(&g_ui, right_x, 0, right_w, box_h, 0, ARDUBOT_COLOR_BG);
            app_display_text_color(&g_ui.ctx.display, right_x + (right_w - num_w) / 2,
                                   num_y + g_ui.ui.content_y, line, num_scale, num_color);
            app_display_text_color(&g_ui.ctx.display, right_x + (right_w - caption_w) / 2,
                                   caption_y + g_ui.ui.content_y, caption, caption_scale,
                                   running ? ARDUBOT_COLOR_TEXT_MUTED
                                           : ARDUBOT_COLOR_WARNING);
        }

        /* Depleting bar along the bottom, full width under both zones -
         * reusable component (apps/app_ui.c), same one counter_app.c's
         * gauge is built from. */
        {
            const int fill_w = (int)(((int64_t)remaining * g_ui.ui.content_w) / phase_total);
            uint16_t bar_color = remaining <= POMODORO_LOW_S  ? ARDUBOT_COLOR_DANGER
                                 : remaining <= POMODORO_MID_S ? ARDUBOT_COLOR_WARNING
                                                                : phase_color;
            app_ui_bar(&g_ui, box_h + 6, bar_h, fill_w, ARDUBOT_COLOR_TRACK, bar_color);
        }

        app_display_flush(&g_ui.ctx.display); /* app_ui_end_frame()'s job - needed on sim too */
    }
}

static void on_cleanup(void* app) {
    (void)app;
    g_worker_alive = false;
    if (g_sec_task) {
        /* A task_sleep()-based worker can be deleted directly, same as
         * stopwatch_app.c's g_sec_task — no task_resume() needed first
         * (that's only for task_suspend()-parked tasks). */
        task_delete(g_sec_task);
        g_sec_task = NULL;
    }
    app_ui_deinit(&g_ui);
    APP_INFO("Pomodoro worker stopped");
}

APP_DEFINE(pomodoro_app, "pomodoro", .version = "1.0.0", .author = "ArdubotOS",
           .description = "Pomodoro countdown timer", .type = APP_TYPE_TOOL,
           .icon = &pomodoro_app_icon, .fps = 30,
           .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
