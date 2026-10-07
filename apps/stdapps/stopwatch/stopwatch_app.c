#include "app_framework.h"
#include "scheduler.h"
#include "stopwatch.h"

#include <stddef.h>

static stopwatch_t g_sw;
static volatile bool g_workers_alive;

static task_tcb_t *g_sec_task;
static task_tcb_t *g_min_task;
static task_tcb_t *g_hour_task;

void stopwatch_reset(stopwatch_t *sw) {
  if (!sw) {
    return;
  }
  sw->hours = 0;
  sw->minutes = 0;
  sw->seconds = 0;
  sw->running = false;
}

bool stopwatch_tick_second(stopwatch_t *sw) {
  if (!sw || !sw->running) {
    return false;
  }
  sw->seconds++;
  if (sw->seconds >= 60) {
    sw->seconds = 0;
    return true;
  }
  return false;
}

bool stopwatch_tick_minute(stopwatch_t *sw) {
  if (!sw || !sw->running) {
    return false;
  }
  sw->minutes++;
  if (sw->minutes >= 60) {
    sw->minutes = 0;
    return true;
  }
  return false;
}

void stopwatch_tick_hour(stopwatch_t *sw) {
  if (!sw || !sw->running) {
    return;
  }
  sw->hours++;
  if (sw->hours >= 100) {
    sw->hours = 0;
  }
}

static app_helper_t *g_app;

static void notify_dirty(void) {
  app_set_state("stopwatch", &g_sw, sizeof(g_sw));
  app_helper_invalidate(g_app);
}

static void task_seconds(void *arg) {
  (void)arg;
  while (g_workers_alive) {
    task_sleep(1000);
    if (!g_workers_alive) {
      break;
    }
    bool minute_due = false;
    bool changed = false;
    scheduler_lock();
    if (g_sw.running) {
      minute_due = stopwatch_tick_second(&g_sw);
      changed = true;
    }
    scheduler_unlock();
    if (minute_due && g_min_task) {
      task_resume(g_min_task);
    }
    if (changed) {
      notify_dirty();
    }
  }
}

static void task_minutes(void *arg) {
  (void)arg;
  while (g_workers_alive) {
    task_suspend(task_get_current());
    if (!g_workers_alive) {
      break;
    }
    bool hour_due = false;
    scheduler_lock();
    if (g_sw.running) {
      hour_due = stopwatch_tick_minute(&g_sw);
    }
    scheduler_unlock();
    if (hour_due && g_hour_task) {
      task_resume(g_hour_task);
    }
    notify_dirty();
  }
}

static void task_hours(void *arg) {
  (void)arg;
  while (g_workers_alive) {
    task_suspend(task_get_current());
    if (!g_workers_alive) {
      break;
    }
    scheduler_lock();
    stopwatch_tick_hour(&g_sw);
    scheduler_unlock();
    notify_dirty();
  }
}

static void delete_workers(void) {
  g_workers_alive = false;
  if (g_min_task) {
    task_resume(g_min_task);
  }
  if (g_hour_task) {
    task_resume(g_hour_task);
  }
  if (g_sec_task) {
    task_delete(g_sec_task);
    g_sec_task = NULL;
  }
  if (g_min_task) {
    task_delete(g_min_task);
    g_min_task = NULL;
  }
  if (g_hour_task) {
    task_delete(g_hour_task);
    g_hour_task = NULL;
  }
}

static void on_event(app_helper_t *app, app_helper_event_t ev) {
  (void)app;
  if (ev == APP_EV_UP) {
    scheduler_lock();
    g_sw.running = !g_sw.running;
    scheduler_unlock();
    APP_INFO("stopwatch %s", g_sw.running ? "running" : "stopped");
  } else if (ev == APP_EV_SELECT) {
    scheduler_lock();
    stopwatch_reset(&g_sw);
    scheduler_unlock();
    APP_INFO("stopwatch reset");
  }
}

static void on_ready(app_helper_t *app) {
  g_app = app;
  g_workers_alive = true;
  if (!app_helper_has_state(app)) {
    stopwatch_reset(&g_sw);
  }

  if (task_create("sw_sec", task_seconds, NULL, TASK_PRIO_NORMAL, 0,
                  &g_sec_task) != 0 ||
      task_create("sw_min", task_minutes, NULL, TASK_PRIO_NORMAL, 0,
                  &g_min_task) != 0 ||
      task_create("sw_hour", task_hours, NULL, TASK_PRIO_NORMAL, 0,
                  &g_hour_task) != 0) {
    APP_ERROR("Failed to create stopwatch worker tasks");
    delete_workers();
    return;
  }

  APP_INFO("Stopwatch ready");
}

static void on_draw(app_helper_t *app) {
  app_helper_labelf(app, 0, "%02u:%02u:%02u", (unsigned)g_sw.hours,
                    (unsigned)g_sw.minutes, (unsigned)g_sw.seconds);
  app_helper_label(app, 2, g_sw.running ? "RUN" : "STP");
}

static void on_cleanup(app_helper_t *app) {
  (void)app;
  delete_workers();
  APP_INFO("Stopwatch workers stopped");
}

APP_HELPER(stopwatch_app, "stopwatch", .state = &g_sw,
           .state_size = sizeof(g_sw), .on_event = on_event,
           .on_ready = on_ready, .on_draw = on_draw, .on_cleanup = on_cleanup)
