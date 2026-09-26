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
    if (!app || !app_kit_is_foreground(app)) {
        return;
    }
    app_display_clear(&app->display);
}

void app_text(app_ctx_t* app, int x, int y, const char* text) {
    if (!app || !text || !app_kit_is_foreground(app)) {
        return;
    }
    app_display_text(&app->display, x, y, text);
}

void app_textf(app_ctx_t* app, int x, int y, const char* fmt, ...) {
    char buf[96];
    va_list args;
    if (!app || !fmt || !app_kit_is_foreground(app)) {
        return;
    }
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    app_display_text(&app->display, x, y, buf);
}

void app_flush(app_ctx_t* app) {
    if (!app || !app_kit_is_foreground(app)) {
        return;
    }
    app_display_flush(&app->display);
    app_clear_dirty(app);
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

void app_request_exit(app_ctx_t* app) {
    if (app) {
        app->running = false;
    }
}

static app_catalog_entry_t g_catalog[APP_KIT_CATALOG_MAX];
static int g_catalog_count = 0;

void app_kit_catalog_clear(void) {
    g_catalog_count = 0;
    memset(g_catalog, 0, sizeof(g_catalog));
}

int app_kit_catalog_build(const char* exclude_name) {
    app_t* apps[APP_KIT_CATALOG_MAX];
    size_t count = 0;

    app_kit_catalog_clear();

    if (app_list(apps, APP_KIT_CATALOG_MAX, &count) != 0) {
        return -1;
    }

    for (size_t i = 0; i < count && g_catalog_count < APP_KIT_CATALOG_MAX; i++) {
        if (!apps[i] || !apps[i]->name) {
            continue;
        }
        if (exclude_name && strcmp(apps[i]->name, exclude_name) == 0) {
            continue;
        }
        g_catalog[g_catalog_count].name = apps[i]->name;
        g_catalog[g_catalog_count].type = apps[i]->type;
        g_catalog_count++;
    }

    return g_catalog_count;
}

int app_kit_catalog_count(void) {
    return g_catalog_count;
}

const app_catalog_entry_t* app_kit_catalog_at(int index) {
    if (index < 0 || index >= g_catalog_count) {
        return NULL;
    }
    return &g_catalog[index];
}

const char* app_type_tag(app_type_t type) {
    switch (type) {
        case APP_TYPE_SYSTEM:
            return "[SYS]";
        case APP_TYPE_USER:
            return "[USR]";
        case APP_TYPE_GAME:
            return "[GME]";
        case APP_TYPE_TOOL:
            return "[TOL]";
        default:
            return "";
    }
}

void app_menu_init(app_menu_t* menu, int start_y, int row_h) {
    if (!menu) {
        return;
    }
    memset(menu, 0, sizeof(*menu));
    menu->start_y = start_y > 0 ? start_y : 16;
    menu->row_h = row_h > 0 ? row_h : 12;
}

void app_menu_clear(app_menu_t* menu) {
    if (!menu) {
        return;
    }
    menu->count = 0;
    menu->selected = 0;
    menu->first_visible = 0;
}

int app_menu_add(app_menu_t* menu, const char* id, const char* label, const char* tag) {
    if (!menu || !id || !label || menu->count >= APP_KIT_MENU_MAX) {
        return -1;
    }
    menu->items[menu->count].id = id;
    menu->items[menu->count].label = label;
    menu->items[menu->count].tag = tag;
    menu->count++;
    return 0;
}

int app_menu_load_catalog(app_menu_t* menu) {
    if (!menu) {
        return -1;
    }
    app_menu_clear(menu);
    for (int i = 0; i < g_catalog_count; i++) {
        const app_catalog_entry_t* e = &g_catalog[i];
        if (app_menu_add(menu, e->name, e->name, app_type_tag(e->type)) != 0) {
            break;
        }
    }
    return menu->count;
}

static int app_menu_visible_rows(const app_menu_t* menu, int display_h) {
    if (!menu || menu->row_h <= 0) {
        return 0;
    }
    int rows = (display_h - menu->start_y) / menu->row_h;
    if (rows < 0) {
        rows = 0;
    }
    if (rows > menu->count) {
        rows = menu->count;
    }
    return rows;
}

bool app_menu_move(app_menu_t* menu, int delta, int display_h) {
    int visible;
    if (!menu || menu->count <= 0 || delta == 0) {
        return false;
    }

    int next = menu->selected + delta;
    if (next < 0) {
        next = 0;
    }
    if (next >= menu->count) {
        next = menu->count - 1;
    }
    if (next == menu->selected) {
        return false;
    }

    menu->selected = next;
    visible = app_menu_visible_rows(menu, display_h);
    if (visible <= 0) {
        return true;
    }
    if (menu->selected < menu->first_visible) {
        menu->first_visible = menu->selected;
    } else if (menu->selected >= menu->first_visible + visible) {
        menu->first_visible = menu->selected - visible + 1;
    }
    return true;
}

const app_menu_item_t* app_menu_selected(const app_menu_t* menu) {
    if (!menu || menu->selected < 0 || menu->selected >= menu->count) {
        return NULL;
    }
    return &menu->items[menu->selected];
}

void app_menu_draw(app_ctx_t* app, app_menu_t* menu, int display_h, const char* title,
                   const char* help) {
    int visible;
    if (!app || !menu) {
        return;
    }

    visible = app_menu_visible_rows(menu, display_h);

    app_clear(app);
    if (title) {
        app_text(app, 0, 0, title);
    }

    for (int i = 0; i < visible; i++) {
        int idx = menu->first_visible + i;
        int y;
        if (idx < 0 || idx >= menu->count) {
            break;
        }
        y = menu->start_y + i * menu->row_h;
        app_textf(app, 0, y, "%c %s", (idx == menu->selected) ? '>' : ' ',
                  menu->items[idx].label ? menu->items[idx].label : "?");
        if (menu->items[idx].tag) {
            app_text(app, 72, y, menu->items[idx].tag);
        }
    }

    if (help) {
        int help_y = menu->start_y + visible * menu->row_h;
        if (help_y < display_h - 8) {
            app_text(app, 0, help_y, help);
        }
    }

    app_flush(app);
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
        /* Child already started; best-effort leave it running. */
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
        /* Bindings registered in on_init — claim keys now that we are focused. */
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
