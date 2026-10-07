#include "app_kit.h"
#include "app_framework.h"
#include "app_ui.h"
#include "fw/io.h"
#include "fw/ui.h"
#include "header_app.h"
#include "icons.h"
#include "notify_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_KIT_HOME_NAME "launcher"
#define APP_KIT_PIN_BASE 20
#define APP_KIT_MAX_CTX 8

static app_ctx_t *g_fg = NULL;
static app_ctx_t *g_home = NULL;
int g_next_pin = APP_KIT_PIN_BASE;

typedef struct {
  const char *name;
  app_ctx_t *ctx;
} app_kit_ctx_slot_t;

static app_kit_ctx_slot_t g_ctx_reg[APP_KIT_MAX_CTX];

static void app_kit_register_ctx(const char *name, app_ctx_t *ctx) {
  if (!name || !ctx) {
    return;
  }
  for (int i = 0; i < APP_KIT_MAX_CTX; i++) {
    if (g_ctx_reg[i].name && strcmp(g_ctx_reg[i].name, name) == 0) {
      g_ctx_reg[i].ctx = ctx;
      return;
    }
  }
  for (int i = 0; i < APP_KIT_MAX_CTX; i++) {
    if (!g_ctx_reg[i].name) {
      g_ctx_reg[i].name = name;
      g_ctx_reg[i].ctx = ctx;
      return;
    }
  }
}

static void app_kit_unregister_ctx(const char *name) {
  if (!name) {
    return;
  }
  for (int i = 0; i < APP_KIT_MAX_CTX; i++) {
    if (g_ctx_reg[i].name && strcmp(g_ctx_reg[i].name, name) == 0) {
      g_ctx_reg[i].name = NULL;
      g_ctx_reg[i].ctx = NULL;
      return;
    }
  }
}

static app_ctx_t *app_kit_find_ctx(const char *name) {
  if (!name) {
    return NULL;
  }
  for (int i = 0; i < APP_KIT_MAX_CTX; i++) {
    if (g_ctx_reg[i].name && strcmp(g_ctx_reg[i].name, name) == 0) {
      return g_ctx_reg[i].ctx;
    }
  }
  return NULL;
}

static void app_kit_remap_keys(app_ctx_t *app) {
  if (!app) {
    return;
  }
  for (uint32_t i = 0; i < app->key_count; i++) {
    sim_gpio_set_key_mapping(app->keys[i].key, app->keys[i].pin, true);
  }
}

static void publish_focus_demand(const app_ctx_t *app) {
  power_demand_t demand = POWER_DEMAND_NORMAL;
  if (app && app->demand_set) {
    demand = app->demand;
  }
  if (app && app->demand_task) {
    (void)power_set_demand(app->demand_task, demand);
  }
  power_governor_set_foreground_demand(demand);
}

static void app_kit_focus(app_ctx_t *app) {
  if (g_fg && g_fg != app && g_fg->demand_task) {
    (void)power_set_demand(g_fg->demand_task, POWER_DEMAND_NORMAL);
  }
  g_fg = app;
  publish_focus_demand(app);
  if (app) {
    app_kit_remap_keys(app);
    app_mark_dirty(app);
  }
}

int app_kit_set_demand(app_ctx_t *app, power_demand_t demand) {
  task_tcb_t *current;
  if (!app || demand == POWER_DEMAND_UNSET || demand > POWER_DEMAND_HIGH) {
    return -1;
  }
  app->demand = demand;
  app->demand_set = true;
  current = task_get_current();
  if (!app->demand_task && current && current->priority != TASK_PRIO_IDLE) {
    app->demand_task = current;
  }
  if (app_kit_is_foreground(app)) {
    if (app->demand_task) {
      (void)power_set_demand(app->demand_task, demand);
    }
    power_governor_set_foreground_demand(demand);
  }
  return 0;
}

power_demand_t app_kit_get_demand(const app_ctx_t *app) {
  if (!app || !app->demand_set) {
    return POWER_DEMAND_NORMAL;
  }
  return app->demand;
}

bool app_kit_is_foreground(const app_ctx_t *app) {
  if (!app) {
    return false;
  }
  /* app_helper keeps its own app_ctx_t copy. app_kit_run() focuses the
   * kit ctx, and the copy shares that desc, so canvas draws from the
   * helper (the launcher, among others) still count as foreground. */
  if (app == g_fg) {
    return true;
  }
  return app_kit_is_foreground_desc(app->desc);
}

bool app_kit_is_foreground_desc(const app_desc_t *desc) {
  /* Gesture bindings hold the app_ui_t, not the kit context. desc is
   * the same pointer app_kit_focus() stored on g_fg. */
  return desc != NULL && g_fg != NULL && g_fg->desc == desc;
}

static void kit_key_trampoline(int pin, void *arg) {
  app_key_binding_t *binding = (app_key_binding_t *)arg;
  (void)pin;
  if (!binding || !binding->fn || !binding->app) {
    return;
  }
  if (!app_kit_is_foreground(binding->app)) {
    return;
  }
  binding->fn(binding->app, binding->user);
}

int app_bind_key(app_ctx_t *app, sim_key_t key, app_key_fn_t fn, void *user) {
  if (!app || !fn) {
    return -1;
  }
  if (app->key_count >= APP_KIT_MAX_KEYS) {
    return -1;
  }

  app_key_binding_t *binding = &app->keys[app->key_count];
  binding->key = key;
  binding->pin = g_next_pin++;
  binding->fn = fn;
  binding->user = user;
  binding->app = app;

  app_button_t btn =
      APP_BUTTON(binding->pin, key, kit_key_trampoline, NULL, binding);
  if (app_button_init(&btn) != 0) {
    return -1;
  }

  app->key_count++;

  if (app_kit_is_foreground(app)) {
    sim_gpio_set_key_mapping(key, binding->pin, true);
  }

  return 0;
}

void app_request_exit_key(app_ctx_t *app, void *user) {
  (void)user;
  app_request_exit(app);
}

int app_bind_back(app_ctx_t *app) {
  return app_bind_key(app, SIM_KEY_ESCAPE, (app_key_fn_t)app_request_exit_key,
                      NULL);
}

void app_request_exit(app_ctx_t *app) {
  if (!app) {
    return;
  }

  /* Home app should not quit - restart it instead. Other apps soft-leave: stay
   * alive in the background. */
  if (!app->desc || !app->desc->name ||
      strcmp(app->desc->name, APP_KIT_HOME_NAME) == 0) {
    /* For home app, just re-focus it (it stays running) */
    if (g_home) {
      app_kit_focus(g_home);
    }
    return;
  }

  /* Resume/focus home BEFORE suspending self: app_suspend() on the
   * currently-running task context-switches away immediately (it never
   * "returns" until this same task is resumed again), so anything placed
   * after it here would only run much later, reacting to a stale escape
   * press right as this app happens to be resumed for an unrelated
   * reason - which is what made back-navigation land on a leftover
   * previous app instead of the OS/launcher. */
  app_t *home = app_find(APP_KIT_HOME_NAME);
  if (home) {
    if (home->state == APP_STATE_SUSPENDED) {
      app_resume(APP_KIT_HOME_NAME);
    } else if (home->state == APP_STATE_INSTALLED ||
               home->state == APP_STATE_STOPPED) {
      app_start(APP_KIT_HOME_NAME);
    }
  }

  if (g_home) {
    app_kit_focus(g_home);
  }

  if (app_suspend(app->desc->name) != 0) {
    app->running = false;
  }
}

const char *app_kit_foreground_name(void) {
  if (!g_fg || !g_fg->desc) {
    return NULL;
  }
  return g_fg->desc->name;
}

int app_kit_switch(const char *name) {
  const char *from_name;
  app_t *target;
  app_ctx_t *to;
  if (!name || name[0] == '\0') {
    return -1;
  }
  target = app_find(name);
  if (!target) {
    return -1;
  }
  from_name = app_kit_foreground_name();
  if (target->state == APP_STATE_SUSPENDED) {
    if (app_resume(name) != 0) {
      return -1;
    }
  } else if (target->state != APP_STATE_RUNNING) {
    if (app_start(name) != 0) {
      return -1;
    }
  }
  to = app_kit_find_ctx(name);
  if (to) {
    app_kit_focus(to);
  }
  if (from_name && strcmp(from_name, name) != 0) {
    (void)app_suspend(from_name);
  }
  return 0;
}

static void app_kit_return_home(const char *leaving);

int app_kit_request_stop(const char *name) {
  app_ctx_t *ctx;
  app_t *app;
  int was_fg;
  if (!name || name[0] == '\0') {
    return -1;
  }
  ctx = app_kit_find_ctx(name);
  app = app_find(name);
  if (!ctx || !app) {
    return -1;
  }
  if (app->state != APP_STATE_RUNNING && app->state != APP_STATE_SUSPENDED) {
    return -1;
  }
  was_fg = (g_fg == ctx);
  ctx->retired = true;
  ctx->running = false;
  if (g_fg == ctx) {
    if (ctx->demand_task) {
      (void)power_set_demand(ctx->demand_task, POWER_DEMAND_NORMAL);
    }
    g_fg = NULL;
    power_governor_set_foreground_demand(POWER_DEMAND_NORMAL);
  }
  if (was_fg) {
    app_kit_return_home(name);
  }
  if (app->state == APP_STATE_SUSPENDED) {
    (void)app_resume(name);
  }
  app_kit_wake(ctx);
  return 0;
}

int app_open(app_ctx_t *from, const char *name) {
  if (!from || !from->desc || !from->desc->name || !name) {
    return -1;
  }
  if (strcmp(from->desc->name, name) == 0) {
    return 0;
  }

  app_t *target = app_find(name);
  if (target && target->state == APP_STATE_SUSPENDED) {
    if (app_resume(name) != 0) {
      return -1;
    }
  } else if (target && target->state == APP_STATE_RUNNING) {
    /* Already running — just focus below. */
  } else {
    if (app_start(name) != 0) {
      return -1;
    }
  }

  /* Focus the target BEFORE suspending the caller - see the comment in
   * app_request_exit() for why code after a self-suspend never runs when
   * expected. This also matters for re-opening an already-suspended app:
   * its own app_kit_run() won't call app_kit_focus() again on resume, so
   * without this, the focus would silently stay on whatever was focused
   * before. */
  app_ctx_t *to_ctx = app_kit_find_ctx(name);
  if (to_ctx) {
    app_kit_focus(to_ctx);
  }

  if (app_suspend(from->desc->name) != 0) {
    APP_WARN("app_open: failed to suspend %s", from->desc->name);
  }

  return 0;
}

static void app_kit_return_home(const char *leaving) {
  if (leaving && strcmp(leaving, APP_KIT_HOME_NAME) == 0) {
    return;
  }

  app_t *home = app_find(APP_KIT_HOME_NAME);
  if (!home) {
    return;
  }

  if (home->state == APP_STATE_SUSPENDED) {
    app_resume(APP_KIT_HOME_NAME);
  } else if (home->state == APP_STATE_INSTALLED ||
             home->state == APP_STATE_STOPPED) {
    app_start(APP_KIT_HOME_NAME);
  }

  if (g_home) {
    app_kit_focus(g_home);
  }
}

app_manifest_t *app_kit_make_manifest(const app_desc_t *desc,
                                      void (*entry)(void)) {
  if (!desc || !desc->name || !entry) {
    return NULL;
  }

  app_manifest_t *manifest = (app_manifest_t *)calloc(1, sizeof(*manifest));
  if (!manifest) {
    return NULL;
  }

  static const capability_t basic_caps[] = APP_CAPS_BASIC;

  manifest->type = desc->type;
  manifest->min_os_version = 1;
  manifest->entry_point = (uintptr_t)entry;
  manifest->stack_size =
      desc->stack_size > 0 ? desc->stack_size : APP_STACK_SMALL;
  manifest->heap_size = desc->heap_size > 0 ? desc->heap_size : APP_HEAP_SMALL;

  strncpy(manifest->name, desc->name, APP_NAME_MAX - 1);
  if (desc->version) {
    strncpy(manifest->version, desc->version, 15);
  }
  if (desc->author) {
    strncpy(manifest->author, desc->author, 63);
  }
  if (desc->description) {
    strncpy(manifest->description, desc->description, 255);
  }

  manifest->capability_count =
      (uint32_t)(sizeof(basic_caps) / sizeof(basic_caps[0]));
  for (uint32_t i = 0; i < manifest->capability_count && i < 16; i++) {
    manifest->capabilities[i] = basic_caps[i];
  }

  return manifest;
}

void app_kit_apply_overrides(app_desc_t *dest, const app_desc_t *over) {
  if (!dest || !over) {
    return;
  }
  if (over->name) {
    dest->name = over->name;
  }
  if (over->version) {
    dest->version = over->version;
  }
  if (over->author) {
    dest->author = over->author;
  }
  if (over->description) {
    dest->description = over->description;
  }
  if (over->type != 0) {
    dest->type = over->type;
  }
  if (over->fps != 0) {
    dest->fps = over->fps;
  }
  if (over->stack_size != 0) {
    dest->stack_size = over->stack_size;
  }
  if (over->heap_size != 0) {
    dest->heap_size = over->heap_size;
  }
  if (over->on_init) {
    dest->on_init = over->on_init;
  }
  if (over->on_frame) {
    dest->on_frame = over->on_frame;
  }
  if (over->on_cleanup) {
    dest->on_cleanup = over->on_cleanup;
  }
  if (over->on_load) {
    dest->on_load = over->on_load;
  }
  if (over->icon) {
    dest->icon = over->icon;
  }
}

typedef struct {
  const char *name;
  const app_icon_t *icon;
} app_kit_icon_slot_t;

static app_kit_icon_slot_t g_icons[APP_MAX];
static int g_icon_count = 0;

void app_kit_set_icon(const char *name, const app_icon_t *icon) {
  int i;
  if (!name || !icon) {
    return;
  }
  for (i = 0; i < g_icon_count; i++) {
    if (g_icons[i].name && strcmp(g_icons[i].name, name) == 0) {
      g_icons[i].icon = icon;
      return;
    }
  }
  if (g_icon_count >= APP_MAX) {
    return;
  }
  g_icons[g_icon_count].name = name;
  g_icons[g_icon_count].icon = icon;
  g_icon_count++;
}

typedef struct {
  const char *name;
  app_load_fn_t fn;
  bool loaded;
} app_kit_load_slot_t;

static app_kit_load_slot_t g_loads[APP_MAX];
static int g_load_count = 0;

void app_kit_set_load(const char *name, app_load_fn_t fn) {
  int i;
  if (!name || !fn) {
    return;
  }
  for (i = 0; i < g_load_count; i++) {
    if (g_loads[i].name && strcmp(g_loads[i].name, name) == 0) {
      if (!g_loads[i].loaded) {
        g_loads[i].fn = fn;
      }
      return;
    }
  }
  if (g_load_count >= APP_MAX) {
    return;
  }
  g_loads[g_load_count].name = name;
  g_loads[g_load_count].fn = fn;
  g_loads[g_load_count].loaded = false;
  g_load_count++;
}

int app_kit_load(const char *name) {
  int i;
  if (!name) {
    return -1;
  }
  for (i = 0; i < g_load_count; i++) {
    if (g_loads[i].name && strcmp(g_loads[i].name, name) == 0) {
      if (!g_loads[i].loaded && g_loads[i].fn) {
        g_loads[i].loaded = true;
        g_loads[i].fn();
      }
      return 0;
    }
  }
  return 0;
}

const app_icon_t *app_kit_get_icon(const char *name) {
  int i;
  if (!name) {
    return &app_icon_default;
  }
  for (i = 0; i < g_icon_count; i++) {
    if (g_icons[i].name && strcmp(g_icons[i].name, name) == 0) {
      return g_icons[i].icon ? g_icons[i].icon : &app_icon_default;
    }
  }
  return &app_icon_default;
}

uint32_t app_kit_next_sleep_ms(const app_ctx_t *app) {
  uint32_t wait = 60000;
  uint32_t fps;
  if (!app) {
    return 1;
  }
  if (app->paced) {
    fps = (app->desc && app->desc->fps > 0) ? app->desc->fps
                                            : (uint32_t)ARDUBOT_DEFAULT_FPS;
    wait = fps > 0 ? 1000u / fps : 1;
    if (wait == 0) {
      wait = 1;
    }
  } else if (app->every_ms > 0) {
    wait = app->every_ms;
  }
  /* The header clock is painted from wall time at flush. Wake at least
   * once a second while that band is up so the body is drawn with it. */
  if (header_app_visible() && app_header_height() > 0 &&
      app->ui.mode == APP_UI_MODE_UI && wait > 1000) {
    wait = 1000;
  }
  if (notify_service_needs_present() && wait > 16) {
    wait = 16;
  }
  return wait;
}

void app_kit_wake(app_ctx_t *app) {
  task_tcb_t *task;
  uint32_t now;
  if (!app) {
    return;
  }
  task = app->run_task;
  if (!task) {
    return;
  }
  if (task->state == TASK_STATE_BLOCKED) {
    now = scheduler_get_tick_count();
    if (task->wake_time > now) {
      task->wake_time = now;
    }
  }
  task_wake(task);
}

void app_kit_wake_foreground(void) { app_kit_wake(g_fg); }

void app_kit_run(const app_desc_t *desc) {
  if (!desc) {
    return;
  }

  app_ctx_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.desc = desc;
  ctx.dirty = true;
  ctx.running = true;

  /* ARDUBOT_DEFAULT_FPS (apps/ui/components/display.h) is the device-
   * configured default (device_config_esp32c6.yaml's app_kit.default_fps)
   * - an app's own manifest .fps, when set, always wins over it. */
  uint32_t fps = desc->fps > 0 ? desc->fps : ARDUBOT_DEFAULT_FPS;

  if (app_display_init(&ctx.display, "/dev/display0") != 0) {
    APP_ERROR("app_kit: display init failed for %s",
              desc->name ? desc->name : "?");
    return;
  }
  app_timer_init(&ctx.timer, fps);

  /* Initialize UI config based on display size */
  if (APP_DISPLAY_HEIGHT > 64) {
    app_ui_config_ui(&ctx.ui, desc->name, "");
  } else {
    app_ui_config_game(&ctx.ui);
  }
  ctx.ui.text_scale = (APP_DISPLAY_HEIGHT > 64) ? 2 : 1;

  if (desc->name && strcmp(desc->name, APP_KIT_HOME_NAME) == 0) {
    g_home = &ctx;
  }

  app_kit_register_ctx(desc->name, &ctx);
  app_kit_focus(&ctx);
  ctx.run_task = task_get_current();

  if (desc->on_init) {
    desc->on_init((void *)&ctx);
    app_kit_remap_keys((void *)&ctx);
  }

  while (ctx.running && !ctx.retired) {
    /* Re-take focus after resume from background. */
    if (app_kit_find_ctx(desc->name) == &ctx && g_fg != &ctx) {
      app_t *self = desc->name ? app_find(desc->name) : NULL;
      if (self && self->state == APP_STATE_RUNNING) {
        app_kit_focus((void *)&ctx);
      }
    }
    notify_service_pump();
    if (notify_service_needs_present()) {
      ctx.dirty = true;
    }
    if (ctx.retired || !ctx.running) {
      break;
    }
    if (desc->on_frame) {
      desc->on_frame((void *)&ctx);
    }
    if (!ctx.running) {
      break;
    }
    task_sleep(app_kit_next_sleep_ms(&ctx));
  }

  if (desc->on_cleanup) {
    desc->on_cleanup((void *)&ctx);
  }
  app_display_deinit(&ctx.display);
  app_kit_unregister_ctx(desc->name);

  if (g_fg == &ctx) {
    if (ctx.demand_task) {
      (void)power_set_demand(ctx.demand_task, POWER_DEMAND_NORMAL);
    }
    g_fg = NULL;
    power_governor_set_foreground_demand(POWER_DEMAND_NORMAL);
  }
  if (g_home == &ctx) {
    g_home = NULL;
  }

  app_kit_return_home(desc->name);
}
