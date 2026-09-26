#include "app_kit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_mark_dirty(app_ctx_t* app) {
    if (app) {
        app->dirty = true;
    }
}

void app_clear_dirty(app_ctx_t* app) {
    if (app) {
        app->dirty = false;
    }
}

bool app_is_dirty(const app_ctx_t* app) {
    return app ? app->dirty : false;
}

void app_clear(app_ctx_t* app) {
    if (!app) {
        return;
    }
    app_display_clear(&app->display);
}

void app_text(app_ctx_t* app, int x, int y, const char* text) {
    if (!app || !text) {
        return;
    }
    app_display_text(&app->display, x, y, text);
}

void app_textf(app_ctx_t* app, int x, int y, const char* fmt, ...) {
    char buf[96];
    va_list args;
    if (!app || !fmt) {
        return;
    }
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    app_display_text(&app->display, x, y, buf);
}

void app_flush(app_ctx_t* app) {
    if (!app) {
        return;
    }
    app_display_flush(&app->display);
    app_clear_dirty(app);
}

static void kit_key_trampoline(int pin, void* arg) {
    app_key_binding_t* binding = (app_key_binding_t*)arg;
    (void)pin;
    if (binding && binding->fn && binding->app) {
        binding->fn(binding->app, binding->user);
    }
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
    binding->pin = (int)app->key_count + 1;
    binding->fn = fn;
    binding->user = user;
    binding->app = app;

    app_button_t btn = APP_BUTTON(binding->pin, key, kit_key_trampoline, NULL, binding);
    if (app_button_init(&btn) != 0) {
        return -1;
    }

    app->key_count++;
    return 0;
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

    if (desc->on_init) {
        desc->on_init(&ctx);
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
}
