#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_MAX_WIDGETS 64
#define UI_MAX_CHILDREN 16
#define UI_MAX_STYLE_PROPS 32
#define UI_THEME_MAX 8

typedef enum {
    UI_WIDGET_CONTAINER = 0,
    UI_WIDGET_LABEL,
    UI_WIDGET_BUTTON,
    UI_WIDGET_IMAGE,
    UI_WIDGET_SLIDER,
    UI_WIDGET_SWITCH,
    UI_WIDGET_CHECKBOX,
    UI_WIDGET_LIST,
    UI_WIDGET_GRIDVIEW,
    UI_WIDGET_TEXTINPUT,
    UI_WIDGET_PROGRESS,
    UI_WIDGET_SPINNER,
    UI_WIDGET_CUSTOM
} ui_widget_type_t;

typedef enum {
    UI_FLEX_DIR_ROW = 0,
    UI_FLEX_DIR_COL,
    UI_FLEX_DIR_ROW_REVERSE,
    UI_FLEX_DIR_COL_REVERSE
} ui_flex_dir_t;

typedef enum {
    UI_JUSTIFY_START = 0,
    UI_JUSTIFY_CENTER,
    UI_JUSTIFY_END,
    UI_JUSTIFY_SPACE_BETWEEN,
    UI_JUSTIFY_SPACE_AROUND,
    UI_JUSTIFY_SPACE_EVENLY
} ui_justify_t;

typedef enum {
    UI_ALIGN_START = 0,
    UI_ALIGN_CENTER,
    UI_ALIGN_END,
    UI_ALIGN_STRETCH,
    UI_ALIGN_BASELINE
} ui_align_t;

typedef enum {
    UI_SIZE_MODE_AUTO = 0,
    UI_SIZE_MODE_FIXED,
    UI_SIZE_MODE_FLEX,
    UI_SIZE_MODE_PERCENT
} ui_size_mode_t;

typedef enum {
    UI_EDGE_LEFT = 0,
    UI_EDGE_TOP,
    UI_EDGE_RIGHT,
    UI_EDGE_BOTTOM
} ui_edge_t;

typedef struct ui_widget ui_widget_t;
typedef struct ui_container ui_container_t;

typedef void (*ui_callback_t)(ui_widget_t* widget, void* arg);
typedef void (*ui_draw_cb_t)(ui_widget_t* widget, void* canvas);
typedef void (*ui_layout_cb_t)(ui_widget_t* widget);

typedef struct {
    uint16_t x, y;
    uint16_t w, h;
} ui_rect_t;

typedef struct {
    uint16_t left, top, right, bottom;
} ui_margin_t;

typedef struct {
    uint8_t r, g, b, a;
} ui_color_t;

typedef struct {
    ui_color_t bg;
    ui_color_t fg;
    ui_color_t border;
    ui_color_t focus;
    ui_color_t disabled;
} ui_colors_t;

typedef struct {
    uint16_t font_size;
    char font_family[32];
    bool bold;
    bool italic;
    uint8_t line_height;
    ui_align_t align;
} ui_text_style_t;

typedef struct {
    ui_size_mode_t width_mode, height_mode;
    uint16_t width, height;
    uint16_t min_w, min_h;
    uint16_t max_w, max_h;
    uint8_t flex_grow;
    uint8_t flex_shrink;
    uint16_t flex_basis;
} ui_layout_params_t;

typedef struct {
    ui_colors_t colors;
    ui_text_style_t text;
    ui_margin_t margin;
    ui_margin_t padding;
    uint8_t border_width;
    uint8_t border_radius;
    uint32_t flags;
} ui_style_t;

struct ui_widget {
    ui_widget_type_t type;
    char name[32];
    ui_rect_t rect;
    ui_layout_params_t layout_params;
    ui_style_t style;
    ui_widget_t* parent;
    ui_widget_t* children[UI_MAX_CHILDREN];
    uint32_t child_count;
    uint32_t ref_count;
    bool visible;
    bool enabled;
    bool focused;
    bool dirty;
    ui_callback_t on_click;
    ui_callback_t on_focus;
    ui_callback_t on_blur;
    ui_callback_t on_value_change;
    ui_draw_cb_t draw;
    ui_layout_cb_t layout_fn;
    void* user_data;
    void* private_data;
};

typedef struct ui_container {
    ui_widget_t widget;
    ui_flex_dir_t flex_dir;
    ui_justify_t justify;
    ui_align_t align;
    uint16_t gap;
} ui_container_t;

typedef struct {
    char name[32];
    ui_style_t base_style;
    ui_color_t colors[16];
    ui_text_style_t text_styles[8];
    uint32_t color_count;
    uint32_t text_style_count;
} ui_theme_t;

typedef struct {
    ui_widget_t* root;
    ui_widget_t* focused;
    ui_widget_t* hover;
    ui_theme_t* theme;
    uint16_t canvas_w, canvas_h;
    uint16_t logical_w, logical_h;
    float scale_x, scale_y;
    input_callback_t input_cb;
    void* input_arg;
} ui_context_t;

ui_context_t* ui_context_create(uint16_t logical_w, uint16_t logical_h, 
                                uint16_t canvas_w, uint16_t canvas_h);
void ui_context_destroy(ui_context_t* ctx);

void ui_context_set_theme(ui_context_t* ctx, ui_theme_t* theme);
ui_theme_t* ui_theme_create_default(void);
void ui_theme_destroy(ui_theme_t* theme);

void ui_theme_set_color(ui_theme_t* theme, uint32_t index, ui_color_t color);
void ui_theme_set_text_style(ui_theme_t* theme, uint32_t index, const ui_text_style_t* style);

ui_widget_t* ui_widget_create(ui_context_t* ctx, ui_widget_type_t type, const char* name);
void ui_widget_destroy(ui_widget_t* widget);

int ui_widget_add_child(ui_widget_t* parent, ui_widget_t* child);
int ui_widget_remove_child(ui_widget_t* parent, ui_widget_t* child);

void ui_widget_set_rect(ui_widget_t* widget, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ui_widget_set_layout(ui_widget_t* widget, const ui_layout_params_t* layout);
void ui_widget_set_style(ui_widget_t* widget, const ui_style_t* style);
void ui_widget_set_margin(ui_widget_t* widget, uint16_t l, uint16_t t, uint16_t r, uint16_t b);
void ui_widget_set_padding(ui_widget_t* widget, uint16_t l, uint16_t t, uint16_t r, uint16_t b);

void ui_widget_set_callback(ui_widget_t* widget, ui_callback_t cb, void* arg);

int ui_widget_focus(ui_widget_t* widget);
void ui_widget_unfocus(ui_widget_t* widget);
ui_widget_t* ui_widget_get_focused(ui_context_t* ctx);

ui_container_t* ui_flex_create(ui_context_t* ctx, ui_flex_dir_t dir, ui_justify_t justify, 
                               ui_align_t align, uint16_t gap);
ui_container_t* ui_grid_create(ui_context_t* ctx, uint8_t cols, uint8_t rows, uint16_t gap);
ui_container_t* ui_scroll_create(ui_context_t* ctx, ui_flex_dir_t dir);

ui_widget_t* ui_label_create(ui_context_t* ctx, const char* text);
ui_widget_t* ui_button_create(ui_context_t* ctx, const char* text, ui_callback_t cb);
ui_widget_t* ui_image_create(ui_context_t* ctx, const void* image_data, uint16_t w, uint16_t h);
ui_widget_t* ui_slider_create(ui_context_t* ctx, uint32_t min, uint32_t max, uint32_t value);
ui_widget_t* ui_switch_create(ui_context_t* ctx, bool value, ui_callback_t cb);
ui_widget_t* ui_checkbox_create(ui_context_t* ctx, const char* label, bool value, ui_callback_t cb);
ui_widget_t* ui_list_create(ui_context_t* ctx, uint32_t (*get_count)(void*), void* (*get_item)(void*, uint32_t));
ui_widget_t* ui_gridview_create(ui_context_t* ctx, uint8_t cols, uint32_t (*get_count)(void*), void* (*get_item)(void*, uint32_t));
ui_widget_t* ui_textinput_create(ui_context_t* ctx, const char* placeholder, ui_callback_t cb);
ui_widget_t* ui_progress_create(ui_context_t* ctx, uint32_t min, uint32_t max, uint32_t value);
ui_widget_t* ui_spinner_create(ui_context_t* ctx);

void ui_render(ui_context_t* ctx);
void ui_layout(ui_context_t* ctx);

void ui_input_event(ui_context_t* ctx, const input_event_t* event);

#ifdef __cplusplus
}
#endif