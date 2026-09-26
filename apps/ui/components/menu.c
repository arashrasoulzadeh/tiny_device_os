#include "menu.h"
#include "app_kit.h"

#include <string.h>

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
    for (int i = 0; i < app_kit_catalog_count(); i++) {
        const app_catalog_entry_t* e = app_kit_catalog_at(i);
        if (!e) {
            break;
        }
        if (app_menu_add(menu, e->name, e->name, app_type_tag(e->type)) != 0) {
            break;
        }
    }
    return menu->count;
}

static int app_menu_visible_rows(const app_menu_t* menu) {
    if (!menu || menu->row_h <= 0) {
        return 0;
    }
    int rows = (APP_DISPLAY_HEIGHT - menu->start_y) / menu->row_h;
    if (rows < 0) {
        rows = 0;
    }
    if (rows > menu->count) {
        rows = menu->count;
    }
    return rows;
}

bool app_menu_move(app_ctx_t* app, app_menu_t* menu, int delta, void* user) {
    int visible;
    int next;
    (void)user;

    if (!menu || menu->count <= 0 || delta == 0) {
        return false;
    }

    next = menu->selected + delta;
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
    visible = app_menu_visible_rows(menu);
    if (visible > 0) {
        if (menu->selected < menu->first_visible) {
            menu->first_visible = menu->selected;
        } else if (menu->selected >= menu->first_visible + visible) {
            menu->first_visible = menu->selected - visible + 1;
        }
    }

    if (app) {
        app_mark_dirty(app);
    }
    return true;
}

const app_menu_item_t* app_menu_selected(const app_menu_t* menu) {
    if (!menu || menu->selected < 0 || menu->selected >= menu->count) {
        return NULL;
    }
    return &menu->items[menu->selected];
}

void app_menu_draw(app_ctx_t* app, app_menu_t* menu, const char* title, const char* help) {
    int visible;
    if (!app || !menu) {
        return;
    }

    visible = app_menu_visible_rows(menu);

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
        if (help_y < APP_DISPLAY_HEIGHT - 8) {
            app_text(app, 0, help_y, help);
        }
    }

    app_flush(app);
}

static void menu_nav_up(app_ctx_t* app, void* user) {
    app_menu_move(app, (app_menu_t*)user, APP_MENU_ONE_UP, user);
}

static void menu_nav_down(app_ctx_t* app, void* user) {
    app_menu_move(app, (app_menu_t*)user, APP_MENU_ONE_DOWN, user);
}

int app_menu_bind_nav(app_ctx_t* app, app_menu_t* menu) {
    if (!app || !menu) {
        return -1;
    }
    if (app_bind_key(app, SIM_KEY_UP, menu_nav_up, menu) != 0) {
        return -1;
    }
    if (app_bind_key(app, SIM_KEY_DOWN, menu_nav_down, menu) != 0) {
        return -1;
    }
    return 0;
}
