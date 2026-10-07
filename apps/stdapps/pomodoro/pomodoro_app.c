#include "app_framework.h"

#define WORK_S (15 * 60)
#define REST_S (5 * 60)
#define DONE_FLASHES 6

typedef struct {
    int phase; /* 0 = work, 1 = rest */
    int32_t remaining_s;
    bool running;
    bool done;
    bool flash_on;
    int flash_count;
} pomodoro_state_t;

static pomodoro_state_t g_st;
static volatile bool g_worker_alive;
static task_tcb_t* g_sec_task;
static app_helper_t* g_app;

static int phase_seconds(int phase) {
    return phase ? REST_S : WORK_S;
}

static const char* phase_name(int phase) {
    return phase ? "REST" : "WORK";
}

static void start_phase(int phase) {
    g_st.phase = phase;
    g_st.remaining_s = phase_seconds(phase);
    g_st.running = true;
    g_st.done = false;
    g_st.flash_on = false;
    g_st.flash_count = 0;
}

static void task_seconds(void* arg) {
    int changed = 0;
    (void)arg;
    while (g_worker_alive) {
        task_sleep(1000);
        if (!g_worker_alive) {
            break;
        }
        scheduler_lock();
        if (g_st.done) {
            g_st.flash_on = !g_st.flash_on;
            if (++g_st.flash_count >= DONE_FLASHES) {
                start_phase(!g_st.phase);
            }
            changed = 1;
        } else if (g_st.running) {
            if (--g_st.remaining_s <= 0) {
                g_st.remaining_s = 0;
                g_st.running = false;
                g_st.done = true;
                g_st.flash_on = true;
                g_st.flash_count = 0;
            }
            changed = 1;
        }
        scheduler_unlock();
        if (changed) {
            app_set_state("pomodoro", &g_st, sizeof(g_st));
            app_helper_invalidate(g_app);
            changed = 0;
        }
    }
}

static void on_event(app_helper_t* app, app_helper_event_t ev) {
    (void)app;
    scheduler_lock();
    if (ev == APP_EV_UP) {
        if (g_st.done) {
            start_phase(!g_st.phase);
        } else {
            g_st.running = !g_st.running;
        }
    } else if (ev == APP_EV_SELECT) {
        start_phase(0);
    }
    scheduler_unlock();
}

void pomodoro_set_worker_demand(task_tcb_t* task) {
    (void)power_set_demand(task, POWER_DEMAND_LOW);
}

static void on_ready(app_helper_t* app) {
    g_app = app;
    g_worker_alive = true;
    if (!app_helper_has_state(app)) {
        start_phase(0);
    }
    if (task_create("pomo_sec", task_seconds, NULL, TASK_PRIO_NORMAL, 0, &g_sec_task) != 0) {
        APP_ERROR("Failed to create pomodoro worker task");
        g_worker_alive = false;
    } else {
        pomodoro_set_worker_demand(g_sec_task);
    }
}

static void on_draw(app_helper_t* app) {
    int32_t remaining;
    int phase;
    int running;
    int done;
    int flash;
    char clock[8];
    const int width = app_helper_content_w(app);
    const int usable = app->ui.ui.content_h - app->ui.ui.content_y;
    const int box_h = usable - 14;
    const int left_w = (width * 2) / 5;
    uint16_t accent;
    uint16_t num_color;

    scheduler_lock();
    remaining = g_st.remaining_s;
    phase = g_st.phase;
    running = g_st.running;
    done = g_st.done;
    flash = g_st.flash_on;
    scheduler_unlock();

    accent = phase ? ARDUBOT_COLOR_ACCENT_COOL : ARDUBOT_COLOR_ACCENT_WARM;
    num_color = app_level_color(remaining, 30, 10);

    if (done) {
        char headline[16];
        snprintf(headline, sizeof(headline), "%s DONE", phase_name(phase));
        app_ui_rect_color(&app->ui, 0, 0, width, app->ui.ui.content_h, 0,
                          flash ? ARDUBOT_COLOR_DANGER : ARDUBOT_COLOR_BG);
        app_helper_center_text(app, 0, 0, width, box_h / 2, headline, 2, ARDUBOT_COLOR_TEXT);
        app_helper_center_text(app, 0, box_h / 2, width, box_h / 2,
                               phase ? "Next: WORK" : "Next: REST", 1, ARDUBOT_COLOR_TEXT_MUTED);
        return;
    }

    app_fmt_clock(clock, sizeof(clock), remaining);
    app_helper_panel(app, 0, 0, left_w, box_h, phase_name(phase), accent);
    app_helper_center_text(app, left_w, 0, width - left_w, box_h * 2 / 3, clock, 4, num_color);
    app_helper_center_text(app, left_w, box_h * 2 / 3, width - left_w, box_h / 3,
                           running ? "LEFT" : "PAUSED", 1,
                           running ? ARDUBOT_COLOR_TEXT_MUTED : ARDUBOT_COLOR_WARNING);
    app_helper_bar(app, box_h + 6, 8, app_bar_fill_px(remaining, phase_seconds(phase), width),
                   remaining <= 30 ? num_color : accent);
}

static void on_cleanup(app_helper_t* app) {
    (void)app;
    g_worker_alive = false;
    if (g_sec_task) {
        task_delete(g_sec_task);
        g_sec_task = NULL;
    }
}

APP_HELPER(pomodoro_app, "pomodoro", .state = &g_st, .state_size = sizeof(g_st),
           .demand = POWER_DEMAND_LOW, .on_event = on_event, .on_ready = on_ready,
           .on_draw = on_draw, .on_cleanup = on_cleanup)
