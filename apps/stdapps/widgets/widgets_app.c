#include "app_framework.h"
#include "app_helper.h"
#include "ui.h"

#include <stdio.h>

/* Widget-framework demo: flex row, label, and button, drawn through the
 * helper's frame instead of a hand-rolled begin/end pair. */

extern const app_icon_t widgets_app_icon;

static app_helper_t* g_app;
static ui_context_t* g_ctx;
static ui_widget_t* g_count_label;
static ui_widget_t* g_button;
static int g_count;

static void update_count_label(void) {
    if (g_count_label) {
        snprintf(g_count_label->name, sizeof(g_count_label->name), "Count: %d", g_count);
    }
}

static void on_tap(ui_widget_t* widget, void* arg) {
    (void)widget;
    (void)arg;
    g_count++;
    update_count_label();
    app_helper_invalidate(g_app);
}

static void on_select(void* app, void* user) {
    (void)app;
    (void)user;
    if (g_button && g_button->on_click) {
        g_button->on_click(g_button, g_button->user_data);
    }
}

static void on_back(void* app, void* user) {
    (void)user;
    app_request_exit(app);
}

static void on_ready(app_helper_t* app) {
    ui_container_t* row;
    g_app = app;

    g_ctx = ui_context_create(APP_DISPLAY_WIDTH, APP_DISPLAY_HEIGHT, APP_DISPLAY_WIDTH,
                              APP_DISPLAY_HEIGHT);
    ui_context_set_display(g_ctx, &app->ui.ctx.display);

    row = ui_flex_create(g_ctx, UI_FLEX_DIR_ROW, UI_JUSTIFY_START, UI_ALIGN_START, 8);
    ui_widget_set_rect((ui_widget_t*)row, 4, 24, APP_DISPLAY_WIDTH - 8, 32);
    ui_widget_set_padding((ui_widget_t*)row, 0, 0, 0, 0);
    ui_widget_add_child(g_ctx->root, (ui_widget_t*)row);

    g_count_label = ui_label_create(g_ctx, "Count: 0");
    g_count_label->layout_params.width_mode = UI_SIZE_MODE_FLEX;
    g_count_label->layout_params.flex_grow = 1;
    ui_widget_add_child((ui_widget_t*)row, g_count_label);

    g_button = ui_button_create(g_ctx, "TAP", on_tap);
    ui_widget_add_child((ui_widget_t*)row, g_button);
    ui_widget_focus(g_button);

    ui_layout(g_ctx);
    APP_INFO("widgets ready");
}

static void on_draw(app_helper_t* app) {
    (void)app;
    ui_render(g_ctx);
}

static void on_cleanup(app_helper_t* app) {
    (void)app;
    ui_context_destroy(g_ctx);
    g_ctx = NULL;
    g_count_label = NULL;
    g_button = NULL;
    g_count = 0;
}

static const app_ui_key_def_t widgets_keys[] = {
    {SIM_KEY_ENTER, on_select, NULL},
    {SIM_KEY_ESCAPE, on_back, NULL},
    {0, NULL, NULL},
};

APP_HELPER(widgets_app, "widgets", .version = "1.0.0", .author = "ArdubotOS", .title = "WIDGETS",
           .help = "Sel:tap  Bk:back",
           .description = "Widget framework demo (flex layout, label/button)",
           .icon = &widgets_app_icon, .fps = 30, .live = true, .keys = widgets_keys,
           .on_ready = on_ready, .on_draw = on_draw, .on_cleanup = on_cleanup)
