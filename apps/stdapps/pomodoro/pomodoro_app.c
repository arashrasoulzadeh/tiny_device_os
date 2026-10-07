#include "app_framework.h"
#include "scheduler.h"

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

static int phase_seconds(int phase) { return phase ? REST_S : WORK_S; }

static const char *phase_name(int phase) { return phase ? "REST" : "WORK"; }

static void start_phase(int phase) {
  g_st.phase = phase;
  g_st.remaining_s = phase_seconds(phase);
  g_st.running = true;
  g_st.done = false;
  g_st.flash_on = false;
  g_st.flash_count = 0;
}

static void on_tick(app_helper_t *app) {
  int changed = 0;
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
    app_helper_invalidate(app);
  }
}

static void on_event(app_helper_t *app, app_helper_event_t ev) {
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

void pomodoro_set_worker_demand(task_tcb_t *task) {
  (void)power_set_demand(task, POWER_DEMAND_LOW);
}

static void on_ready(app_helper_t *app) {
  if (!app_helper_has_state(app)) {
    start_phase(0);
  }
}

static void on_view(app_helper_t *app) {
  int32_t remaining;
  int phase;
  int running;
  int done;
  int flash;
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
    if (flash) {
      app_scene_hero(app, "DONE", ARDUBOT_COLOR_DANGER);
    }
    app_scene_row(app, 0, "%s DONE", phase_name(phase));
    app_scene_row(app, 1, "%s", phase ? "Next: WORK" : "Next: REST");
    return;
  }

  app_scene_panel(app, phase_name(phase), accent);
  app_scene_clock(app, remaining, num_color);
  app_scene_row(app, 0, "%s", running ? "LEFT" : "PAUSED");
  app_scene_bar(app, remaining, phase_seconds(phase),
                remaining <= 30 ? num_color : accent);
}

APP_HELPER(pomodoro_app, "pomodoro", .state = &g_st, .state_size = sizeof(g_st),
           .every_ms = 1000, .demand = POWER_DEMAND_LOW, .on_event = on_event,
           .on_tick = on_tick, .on_ready = on_ready, .on_view = on_view)
