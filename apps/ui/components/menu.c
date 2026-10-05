#include "menu.h"
#include "app_kit.h"
#include "icons.h"
#include "status.h"
#include "os_time.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

/* Flat RGB565 colors for the card-style icon-strip launcher — see
 * boards/esp32-c6-lcd/src/main.cpp's draw_launcher() for the Arduino-side
 * sibling this was ported from (same 320x172 canvas, same layout math).
 * Only meaningful where the display backend actually renders color (the
 * sim's ssd1306_model color overlay) — a plain mono panel just never gets
 * a non-zero rgb565 value looked at.
 *
 * Values come from apps/ui/components/theme.h's shared design tokens
 * (MENU_COLOR_CYAN was already the exact same value as
 * ARDUBOT_COLOR_TITLE_TEXT - not a coincidence, just never pointed at
 * the same constant before) rather than this file's own local literals,
 * so the launcher matches every other app_ui_t-based screen's accent/
 * text/background colors instead of being styled independently. */
#define MENU_COLOR_WHITE ARDUBOT_COLOR_TEXT
#define MENU_COLOR_BLACK ARDUBOT_COLOR_BG
#define MENU_COLOR_CYAN ARDUBOT_COLOR_TITLE_TEXT
#define MENU_COLOR_ORANGE ARDUBOT_COLOR_WARNING

static uint16_t menu_dim_color(uint16_t c, int shift) {
    uint16_t r = (uint16_t)((c >> 11) & 0x1F) >> shift;
    uint16_t g = (uint16_t)((c >> 5) & 0x3F) >> shift;
    uint16_t b = (uint16_t)(c & 0x1F) >> shift;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

typedef struct {
    app_ctx_t* app;
    uint16_t color;
} menu_icon_color_ctx_t;

static void menu_icon_set_pixel_color(int px, int py, bool on, void* user) {
    menu_icon_color_ctx_t* ctx = (menu_icon_color_ctx_t*)user;
    if (!ctx || !ctx->app || !on) {
        return;
    }
    app_pixel_color(ctx->app, px, py, ctx->color);
}

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

/* Card-style icon-strip launcher — ported from
 * boards/esp32-c6-lcd/src/main.cpp's draw_launcher() (same 320x172 canvas,
 * same row layout), but generic over whatever's in the catalog instead of
 * 4 hardcoded apps, and using the shared 1-bit app_icon_t bitmaps (tinted
 * per selection state) rather than hand-authored per-app vector icons,
 * since this code has no way to know what each catalog app "should" look
 * like beyond its bitmap.
 *   y=0..24     status bar: white battery (left), white clock (center)
 *   y=24        separator
 *   y=25..108   carousel: 3 cards (prev/selected/next)
 *   y=109..119  pagination dots, under the selected card
 *   y=120..137  app label (white, 2x text)
 *   y=138..151  subtitle (gray, selected app's tag)
 *   y=152       separator
 *   y=153..172  action bar: [A] Next (orange) / [B] Open (cyan) badges
 * Falls back to a plain single centered icon + label (the old behavior)
 * when the display is smaller than this layout needs, e.g. a 128x32/64
 * mono OLED — this card design needs real screen real estate. */
static void app_menu_draw_icons(app_ctx_t* app, app_menu_t* menu, const char* title,
                                const char* help) {
    const int w = APP_DISPLAY_WIDTH;
    const int h = APP_DISPLAY_HEIGHT;
    const app_menu_item_t* sel = app_menu_selected(menu);

    if (w < 280 || h < 150) {
        /* Small mono panel: keep the old compact single-row icon strip. */
        const int scale = app->ui.text_scale;
        const int icon_size = APP_ICON_SIZE * scale;
        const int icon_pitch = APP_ICON_PITCH * scale;
        const int icon_y = 8 * scale;
        const int label_y = 24 * scale;
        const int center_x = (w / 2) - (APP_ICON_SIZE * scale / 2);
        int side = ((w - APP_STATUS_WIDTH) / icon_pitch) / 2;
        int off;

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
            while (idx < 0) idx += menu->count;
            while (idx >= menu->count) idx -= menu->count;
            x = center_x + off * icon_pitch;
            if (x + icon_size > w - APP_STATUS_WIDTH && off != 0) {
                continue;
            }
            app_icon_blit(x, icon_y, menu->items[idx].icon, off == 0, menu_icon_set_pixel, app,
                         app->ui.text_scale);
        }
        if (sel && sel->label) {
            int len = (int)strlen(sel->label);
            int lx = (w - len * 6) / 2;
            if (lx < 0) lx = 0;
            app_text(app, lx, label_y, sel->label);
        }
        (void)help;
        app_status_draw(app);
        app_flush(app);
        return;
    }

    app_clear(app);

    /* === status bar: y 0..24 === */
    app_draw_rect_color(app, 6, 5, 26, 14, 2, MENU_COLOR_WHITE);
    app_fill_rect_color(app, 32, 9, 3, 6, 0, MENU_COLOR_WHITE);
    {
        int level = (app_status_battery_percent() * 4) / 100;
        int i;
        for (i = 0; i < 4; i++) {
            app_fill_rect_color(app, 9 + i * 5, 8, 3, 8, 0,
                               i < level ? MENU_COLOR_WHITE : menu_dim_color(MENU_COLOR_WHITE, 3));
        }
    }
    {
        char clock_buf[8];
        uint32_t up_s = (uint32_t)(time_now_ms() / 1000);
        snprintf(clock_buf, sizeof(clock_buf), "%02u:%02u", (unsigned)((up_s / 60u) % 100u),
                (unsigned)(up_s % 60u));
        app_text_color(app, (w - app_text_width(clock_buf, 2)) / 2, 5, clock_buf, 2,
                      MENU_COLOR_WHITE);
    }
    app_hline_color(app, 0, 24, w, menu_dim_color(MENU_COLOR_WHITE, 4));

    /* === carousel: y 25..108, 3 cards === */
    {
        const int side_margin = 24;
        const int tile_gap = 12;
        const int side_w = 54;
        const int side_h = 70;
        const int center_w = 140;
        const int center_h = 85;
        const int row_mid = 25 + (108 - 25) / 2;
        const int tile_w[3] = {side_w, center_w, side_w};
        const int tile_h[3] = {side_h, center_h, side_h};
        int tile_x[3];
        int tile_y[3];
        int slot;

        tile_x[1] = (w - center_w) / 2;
        tile_x[0] = tile_x[1] - tile_gap - side_w;
        tile_x[2] = tile_x[1] + center_w + tile_gap;
        tile_y[0] = row_mid - side_h / 2;
        tile_y[1] = row_mid - center_h / 2;
        tile_y[2] = row_mid - side_h / 2;

        for (slot = 0; slot < 3; slot++) {
            int idx = menu->selected + (slot - 1);
            bool is_sel = (slot == 1);
            int icon_size;
            int icon_x, icon_y;
            menu_icon_color_ctx_t icon_ctx;

            while (idx < 0) idx += menu->count;
            while (idx >= menu->count) idx -= menu->count;

            if (is_sel) {
                app_fill_rect_color(app, tile_x[slot], tile_y[slot], tile_w[slot], tile_h[slot], 10,
                                   MENU_COLOR_CYAN);
            } else {
                app_fill_rect_color(app, tile_x[slot], tile_y[slot], tile_w[slot], tile_h[slot], 10,
                                   MENU_COLOR_BLACK);
                app_draw_rect_color(app, tile_x[slot], tile_y[slot], tile_w[slot], tile_h[slot], 10,
                                   menu_dim_color(MENU_COLOR_WHITE, 3));
            }

            icon_size = is_sel ? 4 : 3;
            icon_x = tile_x[slot] + (tile_w[slot] - APP_ICON_SIZE * icon_size) / 2;
            icon_y = tile_y[slot] + (tile_h[slot] - APP_ICON_SIZE * icon_size) / 2;
            icon_ctx.app = app;
            icon_ctx.color = is_sel ? MENU_COLOR_BLACK : menu_dim_color(MENU_COLOR_WHITE, 2);
            app_icon_blit(icon_x, icon_y, menu->items[idx].icon, false, menu_icon_set_pixel_color,
                         &icon_ctx, icon_size);
        }

        if (side_margin > 0) {
            app_text_color(app, side_margin / 2 - 3, row_mid - 3, "<", 2,
                          menu_dim_color(MENU_COLOR_WHITE, 2));
            app_text_color(app, w - side_margin / 2 - 9, row_mid - 3, ">", 2,
                          menu_dim_color(MENU_COLOR_WHITE, 2));
        }

        /* === pagination dots: centered under the selected card === */
        {
            const int dot_pitch = 10;
            const int dots_w = (menu->count - 1) * dot_pitch;
            int dx = tile_x[1] + center_w / 2 - dots_w / 2;
            int bottom = tile_y[1] + center_h;
            int i;
            for (i = 0; i < menu->count; i++) {
                app_fill_circle_color(app, dx, bottom + 7, i == menu->selected ? 3 : 2,
                                     i == menu->selected ? MENU_COLOR_CYAN
                                                         : menu_dim_color(MENU_COLOR_WHITE, 3));
                dx += dot_pitch;
            }
        }
    }

    /* === label + tag === */
    if (sel && sel->label) {
        app_text_color(app, (w - app_text_width(sel->label, 2)) / 2, 120, sel->label, 2,
                      MENU_COLOR_WHITE);
    }
    if (sel && sel->tag) {
        app_text_color(app, (w - app_text_width(sel->tag, 1)) / 2, 142, sel->tag, 1,
                      menu_dim_color(MENU_COLOR_WHITE, 2));
    }

    app_hline_color(app, 0, 152, w, menu_dim_color(MENU_COLOR_WHITE, 4));

    /* === action bar === */
    app_draw_rect_color(app, 16, 155, 16, 16, 3, MENU_COLOR_ORANGE);
    app_text_color(app, 21, 159, "A", 1, MENU_COLOR_ORANGE);
    app_text_color(app, 38, 158, "Next", 2, MENU_COLOR_ORANGE);
    {
        const char* label = "Open";
        int label_w = app_text_width(label, 2);
        int box_x = w - 16 - 16 - 6 - label_w;
        app_draw_rect_color(app, box_x, 155, 16, 16, 3, MENU_COLOR_CYAN);
        app_text_color(app, box_x + 5, 159, "B", 1, MENU_COLOR_CYAN);
        app_text_color(app, box_x + 16 + 6, 158, label, 2, MENU_COLOR_CYAN);
    }

    (void)title;
    (void)help;
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
        app_text_color(app, 0, 0, title, 1, MENU_COLOR_CYAN);
    }

    for (int i = 0; i < visible; i++) {
        int idx = menu->first_visible + i;
        int y;
        bool is_sel;
        char line[40];
        if (idx < 0 || idx >= menu->count) {
            break;
        }
        is_sel = (idx == menu->selected);
        y = menu->start_y + i * menu->row_h;
        if (is_sel) {
            app_fill_rect_color(app, 0, y, APP_DISPLAY_WIDTH, menu->row_h, 0, MENU_COLOR_CYAN);
        }
        snprintf(line, sizeof(line), "%c %s", is_sel ? '>' : ' ',
                menu->items[idx].label ? menu->items[idx].label : "?");
        app_text_color(app, 0, y, line, 1, is_sel ? MENU_COLOR_BLACK : MENU_COLOR_WHITE);
        if (menu->items[idx].tag) {
            app_text_color(app, 72, y, menu->items[idx].tag, 1,
                          is_sel ? MENU_COLOR_BLACK : menu_dim_color(MENU_COLOR_WHITE, 2));
        }
    }

    if (help) {
        int help_y = menu->start_y + visible * menu->row_h;
        if (help_y <= APP_DISPLAY_HEIGHT - 8) {
            app_text_color(app, 0, help_y, help, 1, menu_dim_color(MENU_COLOR_WHITE, 4));
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
    /* Down / Right = next. Up / Left = previous. */
    if (app_bind_key(app, SIM_KEY_UP, menu_nav_prev, menu) != 0) {
        return -1;
    }
    if (app_bind_key(app, SIM_KEY_DOWN, menu_nav_next, menu) != 0) {
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
