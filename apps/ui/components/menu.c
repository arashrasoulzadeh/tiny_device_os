#include "menu.h"
#include "app_kit.h"
#include "icons.h"
#include "status.h"

#include <stdio.h>
#include <string.h>

void app_menu_init(app_menu_t* menu, int start_y, int row_h) {
    if (!menu) {
        return;
    }
    memset(menu, 0, sizeof(*menu));
    menu->start_y = start_y > 0 ? start_y : 16;
    menu->row_h = row_h > 0 ? row_h : 12;
    menu->layout = APP_MENU_LAYOUT_LIST;
}

void app_menu_set_icon_strip(app_menu_t* menu) {
    if (!menu) {
        return;
    }
    menu->layout = APP_MENU_LAYOUT_ICONS;
    menu->first_visible = 0;
}

void app_menu_clear(app_menu_t* menu) {
    if (!menu) {
        return;
    }
    menu->count = 0;
    menu->selected = 0;
    menu->first_visible = 0;
}

int app_menu_add(app_menu_t* menu, const char* id, const char* label, const char* tag,
                 const app_icon_t* icon) {
    if (!menu || !id || !label || menu->count >= APP_KIT_MENU_MAX) {
        return -1;
    }
    menu->items[menu->count].id = id;
    menu->items[menu->count].label = label;
    menu->items[menu->count].tag = tag;
    menu->items[menu->count].icon = icon ? icon : &app_icon_default;
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
        if (app_menu_add(menu, e->name, e->name, app_type_tag(e->type), e->icon) != 0) {
            break;
        }
    }
    return menu->count;
}

static int app_menu_visible_rows(const app_menu_t* menu) {
    if (!menu || menu->row_h <= 0) {
        return 0;
    }
    int avail = APP_DISPLAY_HEIGHT - menu->start_y - 8;
    int rows = avail / menu->row_h;
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
    /* Unlimited wrap in both directions. */
    while (next < 0) {
        next += menu->count;
    }
    while (next >= menu->count) {
        next -= menu->count;
    }
    if (next == menu->selected) {
        return false;
    }

    menu->selected = next;

    if (menu->layout != APP_MENU_LAYOUT_ICONS) {
        visible = app_menu_visible_rows(menu);
        if (visible > 0) {
            if (menu->selected < menu->first_visible) {
                menu->first_visible = menu->selected;
            } else if (menu->selected >= menu->first_visible + visible) {
                menu->first_visible = menu->selected - visible + 1;
            }
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

static void menu_icon_set_pixel(int px, int py, bool on, void* user) {
    app_ctx_t* app = (app_ctx_t*)user;
    if (!app || !on) {
        return;
    }
    app_pixel(app, px, py, true);
}

static void app_menu_draw_icons(app_ctx_t* app, app_menu_t* menu, const char* title,
                                const char* help) {
    const int scale = app->ui.text_scale;
    const int icon_size = APP_ICON_SIZE * scale;
    const int icon_pitch = APP_ICON_PITCH * scale;
    const int icon_y = 8 * scale; /* leave y=0..7 for status bar */
    const int label_y = 24 * scale;
    const int center_x = (APP_DISPLAY_WIDTH / 2) - (APP_ICON_SIZE * scale / 2);
    /* How many icons fit each side of the focused one (keep clear of status). */
    int side = ((APP_DISPLAY_WIDTH - APP_STATUS_WIDTH) / icon_pitch) / 2;
    int off;
    const app_menu_item_t* sel;

    if (side < 1) {
        side = 1;
    }

    app_clear(app);
    if (title) {
        app_text(app, 0, 0, title);
    }

    for (off = -side; off <= side; off++) {
        int idx = menu->selected + off;
        int x;
        while (idx < 0) {
            idx += menu->count;
        }
        while (idx >= menu->count) {
            idx -= menu->count;
        }
        x = center_x + off * icon_pitch;
        if (x + icon_size > APP_DISPLAY_WIDTH - APP_STATUS_WIDTH && off != 0) {
            continue; /* don't cover status icons */
        }
        app_icon_blit(x, icon_y, menu->items[idx].icon, off == 0, menu_icon_set_pixel, app, app->ui.text_scale);
    }

    sel = app_menu_selected(menu);
    if (sel && sel->label) {
        int len = (int)strlen(sel->label);
        int lx = (APP_DISPLAY_WIDTH - len * 6) / 2;
        if (lx < 0) {
            lx = 0;
        }
        app_text(app, lx, label_y, sel->label);
    }

    (void)help;
    app_status_draw(app);
    app_flush(app);

    (void)help;
    app_status_draw(app);
    app_flush(app);
}

void app_menu_draw(app_ctx_t* app, app_menu_t* menu, const char* title, const char* help) {
    int visible;
    if (!app || !menu) {
        return;
    }

    if (menu->layout == APP_MENU_LAYOUT_ICONS) {
        app_menu_draw_icons(app, menu, title, help);
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
        if (help_y <= APP_DISPLAY_HEIGHT - 8) {
            app_text(app, 0, help_y, help);
        }
    }

    app_status_draw(app);
    app_flush(app);
}

static void menu_nav_next(void* app, void* user) {
    app_menu_move((app_ctx_t*)app, (app_menu_t*)user, APP_MENU_ONE_DOWN, user);
}

static void menu_nav_prev(void* app, void* user) {
    app_menu_move((app_ctx_t*)app, (app_menu_t*)user, APP_MENU_ONE_UP, user);
}

int app_menu_bind_nav(app_ctx_t* app, app_menu_t* menu) {
    if (!app || !menu) {
        return -1;
    }
    /* Up / Right = next (side-scroll forward). Down / Left = previous. */
    if (app_bind_key(app, SIM_KEY_UP, menu_nav_next, menu) != 0) {
        return -1;
    }
    if (app_bind_key(app, SIM_KEY_DOWN, menu_nav_prev, menu) != 0) {
        return -1;
    }
    if (app_bind_key(app, SIM_KEY_RIGHT, menu_nav_next, menu) != 0) {
        return -1;
    }
    if (app_bind_key(app, SIM_KEY_LEFT, menu_nav_prev, menu) != 0) {
        return -1;
    }
    return 0;
}
