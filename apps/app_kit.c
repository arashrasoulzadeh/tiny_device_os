#include "app_kit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_KIT_HOME_NAME "launcher"
#define APP_KIT_PIN_BASE 20

static app_ctx_t* g_fg = NULL;
static app_ctx_t* g_home = NULL;
static int g_next_pin = APP_KIT_PIN_BASE;

static void app_kit_remap_keys(app_ctx_t* app) {
    if (!app) {
        return;
    }
    for (uint32_t i = 0; i < app->key_count; i++) {
        sim_gpio_set_key_mapping(app->keys[i].key, app->keys[i].pin, true);
    }
}

static void app_kit_focus(app_ctx_t* app) {
    g_fg = app;
    if (app) {
        app_kit_remap_keys(app);
        app_mark_dirty(app);
    }
}

bool app_kit_is_foreground(const app_ctx_t* app) {
    return app != NULL && app == g_fg;
}

static void kit_key_trampoline(int pin, void* arg) {
    app_key_binding_t* binding = (app_key_binding_t*)arg;
    (void)pin;
    if (!binding || !binding->fn || !binding->app) {
        return;
    }
    if (!app_kit_is_foreground(binding->app)) {
        return;
    }
    binding->fn(binding->app, binding->user);
}

int app_bind_key(app_ctx_t* app, sim_key_t key, app_key_fn_t fn, void* user) {
    if (!app || !fn) {
        return -1;
    }
    if (app->key_count >= APP_KIT_MAX_KEYS) {
        return -1;
    }

    app_key_binding_t* binding = &app->keys[app->key_count];
    binding->key = key;
    binding->pin = g_next_pin++;
    binding->fn = fn;
    binding->user = user;
    binding->app = app;

    app_button_t btn = APP_BUTTON(binding->pin, key, kit_key_trampoline, NULL, binding);
    if (app_button_init(&btn) != 0) {
        return -1;
    }

    app->key_count++;

    if (app_kit_is_foreground(app)) {
        sim_gpio_set_key_mapping(key, binding->pin, true);
    }

    return 0;
}

static void kit_back_trampoline(app_ctx_t* app, void* user) {
    (void)user;
    app_request_exit(app);
}

int app_bind_back(app_ctx_t* app) {
    return app_bind_key(app, SIM_KEY_ESCAPE, kit_back_trampoline, NULL);
}

void app_request_exit(app_ctx_t* app) {
    if (app) {
        app->running = false;
    }
}

int app_open(app_ctx_t* from, const char* name) {
    if (!from || !from->desc || !from->desc->name || !name) {
        return -1;
    }
    if (strcmp(from->desc->name, name) == 0) {
        return 0;
    }

    if (app_start(name) != 0) {
        return -1;
    }

    if (app_suspend(from->desc->name) != 0) {
        APP_WARN("app_open: failed to suspend %s", from->desc->name);
    }

    return 0;
}

static void app_kit_return_home(const char* leaving) {
    if (leaving && strcmp(leaving, APP_KIT_HOME_NAME) == 0) {
        return;
    }

    app_t* home = app_find(APP_KIT_HOME_NAME);
    if (!home) {
        return;
    }

    if (home->state == APP_STATE_SUSPENDED) {
        app_resume(APP_KIT_HOME_NAME);
    } else if (home->state == APP_STATE_INSTALLED || home->state == APP_STATE_STOPPED) {
        app_start(APP_KIT_HOME_NAME);
    }

    if (g_home) {
        app_kit_focus(g_home);
    }
}

app_manifest_t* app_kit_make_manifest(const app_desc_t* desc, void (*entry)(void)) {
    if (!desc || !desc->name || !entry) {
        return NULL;
    }

    app_manifest_t* manifest = (app_manifest_t*)calloc(1, sizeof(*manifest));
    if (!manifest) {
        return NULL;
    }

    static const capability_t basic_caps[] = APP_CAPS_BASIC;

    manifest->type = desc->type;
    manifest->min_os_version = 1;
    manifest->entry_point = (uintptr_t)entry;
    manifest->stack_size = desc->stack_size > 0 ? desc->stack_size : APP_STACK_SMALL;
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

    manifest->capability_count = (uint32_t)(sizeof(basic_caps) / sizeof(basic_caps[0]));
    for (uint32_t i = 0; i < manifest->capability_count && i < 16; i++) {
        manifest->capabilities[i] = basic_caps[i];
    }

    return manifest;
}

void app_kit_apply_overrides(app_desc_t* dest, const app_desc_t* over) {
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
}

void app_kit_run(const app_desc_t* desc) {
    if (!desc) {
        return;
    }

    app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.desc = desc;
    ctx.dirty = true;
    ctx.running = true;

    uint32_t fps = desc->fps > 0 ? desc->fps : 30;

    if (app_display_init(&ctx.display, "/dev/display0") != 0) {
        APP_ERROR("app_kit: display init failed for %s", desc->name ? desc->name : "?");
        return;
    }
    app_timer_init(&ctx.timer, fps);

    if (desc->name && strcmp(desc->name, APP_KIT_HOME_NAME) == 0) {
        g_home = &ctx;
    }

    app_kit_focus(&ctx);

    if (desc->on_init) {
        desc->on_init(&ctx);
        app_kit_remap_keys(&ctx);
    }

    while (ctx.running) {
        if (desc->on_frame) {
            desc->on_frame(&ctx);
        }
        {
            uint32_t frame_ms = 1000u / fps;
            if (frame_ms == 0) {
                frame_ms = 1;
            }
            task_sleep(frame_ms);
        }
    }

    if (desc->on_cleanup) {
        desc->on_cleanup(&ctx);
    }
    app_display_deinit(&ctx.display);

    if (g_fg == &ctx) {
        g_fg = NULL;
    }
    if (g_home == &ctx) {
        g_home = NULL;
    }

    app_kit_return_home(desc->name);
}
