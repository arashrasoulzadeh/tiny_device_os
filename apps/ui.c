#include "ui.h"
#include "input.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define UI_COLOR_WHITE   ((ui_color_t){255, 255, 255, 255})
#define UI_COLOR_BLACK   ((ui_color_t){0, 0, 0, 255})
#define UI_COLOR_RED     ((ui_color_t){255, 0, 0, 255})
#define UI_COLOR_GREEN   ((ui_color_t){0, 255, 0, 255})
#define UI_COLOR_BLUE    ((ui_color_t){0, 0, 255, 255})
#define UI_COLOR_GRAY    ((ui_color_t){128, 128, 128, 255})
#define UI_COLOR_LTGRAY  ((ui_color_t){200, 200, 200, 255})
#define UI_COLOR_DKGRAY  ((ui_color_t){64, 64, 64, 255})

static ui_color_t color_blend(ui_color_t a, ui_color_t b, float t) {
    ui_color_t c;
    c.r = (uint8_t)(a.r + (b.r - a.r) * t);
    c.g = (uint8_t)(a.g + (b.g - a.g) * t);
    c.b = (uint8_t)(a.b + (b.b - a.b) * t);
    c.a = (uint8_t)(a.a + (b.a - a.a) * t);
    return c;
}

static bool rect_contains(const ui_rect_t* rect, int16_t x, int16_t y) {
    return x >= rect->x && x < (int16_t)(rect->x + rect->w) &&
           y >= rect->y && y < (int16_t)(rect->y + rect->h);
}

static void draw_rect(void* canvas, const ui_rect_t* rect, ui_color_t color) {
    (void)canvas; (void)rect; (void)color;
}

static void draw_text(void* canvas, const char* text, int16_t x, int16_t y, ui_color_t color) {
    (void)canvas; (void)text; (void)x; (void)y; (void)color;
}

ui_context_t* ui_context_create(uint16_t logical_w, uint16_t logical_h, 
                                uint16_t canvas_w, uint16_t canvas_h) {
    ui_context_t* ctx = calloc(1, sizeof(ui_context_t));
    if (!ctx) return NULL;
    
    ctx->logical_w = logical_w;
    ctx->logical_h = logical_h;
    ctx->canvas_w = canvas_w;
    ctx->canvas_h = canvas_h;
    ctx->scale_x = (float)canvas_w / logical_w;
    ctx->scale_y = (float)canvas_h / logical_h;
    
    ctx->theme = ui_theme_create_default();
    ctx->root = ui_widget_create(ctx, UI_WIDGET_CONTAINER, "root");
    if (ctx->root) {
        ctx->root->rect = (ui_rect_t){0, 0, logical_w, logical_h};
ctx->root->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    ctx->root->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    ctx->root->layout_params.width = logical_w;
    ctx->root->layout_params.height = logical_h;
    }
    
    return ctx;
}

void ui_context_destroy(ui_context_t* ctx) {
    if (!ctx) return;
    if (ctx->root) ui_widget_destroy(ctx->root);
    if (ctx->theme) ui_theme_destroy(ctx->theme);
    free(ctx);
}

void ui_context_set_theme(ui_context_t* ctx, ui_theme_t* theme) {
    if (!ctx || !theme) return;
    if (ctx->theme) ui_theme_destroy(ctx->theme);
    ctx->theme = theme;
}

ui_theme_t* ui_theme_create_default(void) {
    ui_theme_t* theme = calloc(1, sizeof(ui_theme_t));
    if (!theme) return NULL;
    
    strncpy(theme->name, "default", 31);
    
    theme->base_style = (ui_style_t){
        .colors = {.bg = UI_COLOR_WHITE, .fg = UI_COLOR_BLACK, .border = UI_COLOR_GRAY, .focus = UI_COLOR_BLUE, .disabled = UI_COLOR_DKGRAY},
        .text = {14, "default", false, false, 18, UI_ALIGN_START},
        .margin = {0, 0, 0, 0},
        .padding = {8, 8, 8, 8},
        .border_width = 1,
        .border_radius = 4,
    };
    
    theme->colors[0] = UI_COLOR_WHITE;
    theme->colors[1] = UI_COLOR_BLACK;
    theme->colors[2] = UI_COLOR_BLUE;
    theme->colors[3] = UI_COLOR_RED;
    theme->colors[4] = UI_COLOR_GREEN;
    theme->colors[5] = UI_COLOR_GRAY;
    theme->colors[6] = UI_COLOR_LTGRAY;
    theme->colors[7] = UI_COLOR_DKGRAY;
    theme->color_count = 8;
    
    return theme;
}

void ui_theme_destroy(ui_theme_t* theme) {
    free(theme);
}

void ui_theme_set_color(ui_theme_t* theme, uint32_t index, ui_color_t color) {
    if (theme && index < 16) {
        theme->colors[index] = color;
    }
}

void ui_theme_set_text_style(ui_theme_t* theme, uint32_t index, const ui_text_style_t* style) {
    if (theme && style && index < 8) {
        theme->text_styles[index] = *style;
    }
}

ui_widget_t* ui_widget_create(ui_context_t* ctx, ui_widget_type_t type, const char* name) {
    ui_widget_t* w = calloc(1, sizeof(ui_widget_t));
    if (!w) return NULL;
    
    w->type = type;
    if (name) strncpy(w->name, name, 31);
    w->rect = (ui_rect_t){0, 0, 0, 0};
    w->layout_params = (ui_layout_params_t){
        .width_mode = UI_SIZE_MODE_AUTO,
        .height_mode = UI_SIZE_MODE_AUTO,
        .min_w = 0, .min_h = 0,
        .max_w = 65535, .max_h = 65535,
        .flex_grow = 0, .flex_shrink = 1, .flex_basis = 0
    };
    w->style = (ui_style_t){
        .colors = {.bg = UI_COLOR_WHITE, .fg = UI_COLOR_BLACK, .border = UI_COLOR_GRAY, .focus = UI_COLOR_BLUE, .disabled = UI_COLOR_DKGRAY},
        .text = {14, "default", false, false, 18, UI_ALIGN_START},
        .margin = {0, 0, 0, 0},
        .padding = {8, 8, 8, 8},
        .border_width = 1,
        .border_radius = 4,
    };
    w->visible = true;
    w->enabled = true;
    w->ref_count = 1;
    w->dirty = true;
    
    return w;
}

void ui_widget_destroy(ui_widget_t* widget) {
    if (!widget) return;
    
    for (uint32_t i = 0; i < widget->child_count; i++) {
        ui_widget_destroy(widget->children[i]);
    }
    
    free(widget);
}

int ui_widget_add_child(ui_widget_t* parent, ui_widget_t* child) {
    if (!parent || !child || parent->child_count >= UI_MAX_CHILDREN) return -1;
    
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    parent->dirty = true;
    return 0;
}

int ui_widget_remove_child(ui_widget_t* parent, ui_widget_t* child) {
    if (!parent || !child) return -1;
    
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            for (uint32_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            child->parent = NULL;
            parent->dirty = true;
            return 0;
        }
    }
    return -1;
}

void ui_widget_set_rect(ui_widget_t* widget, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!widget) return;
    widget->rect = (ui_rect_t){x, y, w, h};
    widget->dirty = true;
}

void ui_widget_set_layout(ui_widget_t* widget, const ui_layout_params_t* layout) {
    if (!widget || !layout) return;
    widget->layout_params = *layout;
    widget->dirty = true;
}

void ui_widget_set_style(ui_widget_t* widget, const ui_style_t* style) {
    if (!widget || !style) return;
    widget->style = *style;
    widget->dirty = true;
}

void ui_widget_set_margin(ui_widget_t* widget, uint16_t l, uint16_t t, uint16_t r, uint16_t b) {
    if (!widget) return;
    widget->style.margin = (ui_margin_t){l, t, r, b};
    widget->dirty = true;
}

void ui_widget_set_padding(ui_widget_t* widget, uint16_t l, uint16_t t, uint16_t r, uint16_t b) {
    if (!widget) return;
    widget->style.padding = (ui_margin_t){l, t, r, b};
    widget->dirty = true;
}

void ui_widget_set_callback(ui_widget_t* widget, ui_callback_t cb, void* arg) {
    if (!widget) return;
    widget->on_click = cb;
    widget->user_data = arg;
}

int ui_widget_focus(ui_widget_t* widget) {
    if (!widget || !widget->enabled) return -1;
    widget->focused = true;
    if (widget->on_focus) widget->on_focus(widget, widget->user_data);
    return 0;
}

void ui_widget_unfocus(ui_widget_t* widget) {
    if (!widget) return;
    if (widget->focused) {
        widget->focused = false;
        if (widget->on_blur) widget->on_blur(widget, widget->user_data);
    }
}

ui_widget_t* ui_widget_get_focused(ui_context_t* ctx) {
    return ctx ? ctx->focused : NULL;
}

static void container_layout(ui_widget_t* widget) {
    ui_container_t* cont = (ui_container_t*)widget;
    int16_t x = widget->rect.x + widget->style.padding.left;
    int16_t y = widget->rect.y + widget->style.padding.top;
    int16_t w = widget->rect.w - widget->style.padding.left - widget->style.padding.right;
    int16_t h = widget->rect.h - widget->style.padding.top - widget->style.padding.bottom;
    
    if (cont->flex_dir == UI_FLEX_DIR_ROW || cont->flex_dir == UI_FLEX_DIR_ROW_REVERSE) {
        int16_t total_flex = 0;
        int16_t fixed_w = 0;
        for (uint32_t i = 0; i < widget->child_count; i++) {
            ui_widget_t* child = widget->children[i];
            if (!child->visible) continue;
            if (child->layout_params.width_mode == UI_SIZE_MODE_FLEX) {
                total_flex += child->layout_params.flex_grow;
            } else {
                fixed_w += child->layout_params.width + cont->gap;
            }
        }
        
        int16_t flex_w = w > fixed_w ? w - fixed_w : 0;
        for (uint32_t i = 0; i < widget->child_count; i++) {
            ui_widget_t* child = widget->children[i];
            if (!child->visible) continue;
            
            if (child->layout_params.width_mode == UI_SIZE_MODE_FLEX && total_flex > 0) {
                child->rect.w = (flex_w * child->layout_params.flex_grow) / total_flex;
            } else {
                child->rect.w = child->layout_params.width;
            }
            child->rect.h = child->layout_params.height_mode == UI_SIZE_MODE_FIXED ? 
                           child->layout_params.height : h;
            child->rect.x = x;
            child->rect.y = y;
            x += child->rect.w + cont->gap;
        }
    } else {
        // Column layout
        int16_t total_flex = 0;
        int16_t fixed_h = 0;
        for (uint32_t i = 0; i < widget->child_count; i++) {
            ui_widget_t* child = widget->children[i];
            if (!child->visible) continue;
            if (child->layout_params.height_mode == UI_SIZE_MODE_FLEX) {
                total_flex += child->layout_params.flex_grow;
            } else {
                fixed_h += child->layout_params.height + cont->gap;
            }
        }
        
        int16_t flex_h = h > fixed_h ? h - fixed_h : 0;
        for (uint32_t i = 0; i < widget->child_count; i++) {
            ui_widget_t* child = widget->children[i];
            if (!child->visible) continue;
            
            if (child->layout_params.height_mode == UI_SIZE_MODE_FLEX && total_flex > 0) {
                child->rect.h = (flex_h * child->layout_params.flex_grow) / total_flex;
            } else {
                child->rect.h = child->layout_params.height;
            }
            child->rect.w = child->layout_params.width_mode == UI_SIZE_MODE_FIXED ? 
                           child->layout_params.width : w;
            child->rect.x = x;
            child->rect.y = y;
            y += child->rect.h + cont->gap;
        }
    }
    
    for (uint32_t i = 0; i < widget->child_count; i++) {
        if (widget->children[i]->visible) {
            widget->children[i]->dirty = true;
            if (widget->children[i]->layout_fn) {
                widget->children[i]->layout_fn(widget->children[i]);
            }
        }
    }
}

ui_container_t* ui_flex_create(ui_context_t* ctx, ui_flex_dir_t dir, ui_justify_t justify, 
                               ui_align_t align, uint16_t gap) {
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_CONTAINER, "flex");
    if (!w) return NULL;
    
    ui_container_t* cont = (ui_container_t*)w;
    cont->flex_dir = dir;
    cont->justify = justify;
    cont->align = align;
    cont->gap = gap;
    w->layout_fn = container_layout;
    
    return cont;
}

ui_container_t* ui_grid_create(ui_context_t* ctx, uint8_t cols, uint8_t rows, uint16_t gap) {
    (void)rows;
    return ui_flex_create(ctx, UI_FLEX_DIR_ROW, UI_JUSTIFY_START, UI_ALIGN_START, gap);
}

ui_container_t* ui_scroll_create(ui_context_t* ctx, ui_flex_dir_t dir) {
    return ui_flex_create(ctx, dir, UI_JUSTIFY_START, UI_ALIGN_START, 0);
}

static void label_draw(ui_widget_t* widget, void* canvas) {
    ui_color_t color = widget->enabled ? widget->style.colors.fg : widget->style.colors.disabled;
    draw_text(canvas, widget->name, widget->rect.x, widget->rect.y + widget->rect.h / 2, color);
}

ui_widget_t* ui_label_create(ui_context_t* ctx, const char* text) {
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_LABEL, text);
    if (!w) return NULL;
    w->draw = label_draw;
    return w;
}

static void button_draw(ui_widget_t* widget, void* canvas) {
    ui_color_t bg = widget->focused ? widget->style.colors.focus : 
                    widget->enabled ? widget->style.colors.bg : widget->style.colors.disabled;
    ui_color_t fg = widget->enabled ? widget->style.colors.fg : widget->style.colors.disabled;
    
    draw_rect(canvas, &widget->rect, bg);
    draw_text(canvas, widget->name, 
              widget->rect.x + widget->rect.w / 2, 
              widget->rect.y + widget->rect.h / 2, fg);
}

static void button_click(ui_widget_t* widget, void* arg) {
    if (widget->on_click) widget->on_click(widget, arg);
}

ui_widget_t* ui_button_create(ui_context_t* ctx, const char* text, ui_callback_t cb) {
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_BUTTON, text);
    if (!w) return NULL;
    w->draw = button_draw;
    w->on_click = cb ? cb : button_click;
    w->layout_params.width = 100;
    w->layout_params.height = 32;
    w->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    w->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    return w;
}

static void slider_draw(ui_widget_t* widget, void* canvas) {
    draw_rect(canvas, &widget->rect, widget->style.colors.bg);
    // Would draw slider track and thumb
}

ui_widget_t* ui_slider_create(ui_context_t* ctx, uint32_t min, uint32_t max, uint32_t value) {
    (void)min; (void)max; (void)value;
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_SLIDER, "slider");
    if (!w) return NULL;
    w->draw = slider_draw;
    w->layout_params.width = 200;
    w->layout_params.height = 24;
    w->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    w->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    return w;
}

static void switch_draw(ui_widget_t* widget, void* canvas) {
    draw_rect(canvas, &widget->rect, widget->style.colors.bg);
}

ui_widget_t* ui_switch_create(ui_context_t* ctx, bool value, ui_callback_t cb) {
    (void)value;
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_SWITCH, "switch");
    if (!w) return NULL;
    w->draw = switch_draw;
    w->on_click = cb;
    w->layout_params.width = 52;
    w->layout_params.height = 28;
    w->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    w->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    return w;
}

static void checkbox_draw(ui_widget_t* widget, void* canvas) {
    draw_rect(canvas, &widget->rect, widget->style.colors.bg);
    draw_text(canvas, widget->name, widget->rect.x + 24, widget->rect.y + 4, widget->style.colors.fg);
}

ui_widget_t* ui_checkbox_create(ui_context_t* ctx, const char* label, bool value, ui_callback_t cb) {
    (void)value;
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_CHECKBOX, label);
    if (!w) return NULL;
    w->draw = checkbox_draw;
    w->on_click = cb;
    w->layout_params.width = 200;
    w->layout_params.height = 24;
    w->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    w->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    return w;
}

static void list_draw(ui_widget_t* widget, void* canvas) {
    draw_rect(canvas, &widget->rect, widget->style.colors.bg);
}

ui_widget_t* ui_list_create(ui_context_t* ctx, uint32_t (*get_count)(void*), void* (*get_item)(void*, uint32_t)) {
    (void)get_count; (void)get_item;
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_LIST, "list");
    if (!w) return NULL;
    w->draw = list_draw;
    return w;
}

ui_widget_t* ui_gridview_create(ui_context_t* ctx, uint8_t cols, uint32_t (*get_count)(void*), void* (*get_item)(void*, uint32_t)) {
    (void)cols; (void)get_count; (void)get_item;
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_GRIDVIEW, "gridview");
    if (!w) return NULL;
    return w;
}

static void textinput_draw(ui_widget_t* widget, void* canvas) {
    ui_color_t bg = widget->focused ? widget->style.colors.focus : widget->style.colors.bg;
    draw_rect(canvas, &widget->rect, bg);
    draw_text(canvas, widget->name, widget->rect.x + 4, widget->rect.y + 4, widget->style.colors.fg);
}

ui_widget_t* ui_textinput_create(ui_context_t* ctx, const char* placeholder, ui_callback_t cb) {
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_TEXTINPUT, placeholder);
    if (!w) return NULL;
    w->draw = textinput_draw;
    w->on_value_change = cb;
w->layout_params.width = 200;
    w->layout_params.height = 24;
    w->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    w->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    return w;
}

static void progress_draw(ui_widget_t* widget, void* canvas) {
    draw_rect(canvas, &widget->rect, widget->style.colors.bg);
}

ui_widget_t* ui_progress_create(ui_context_t* ctx, uint32_t min, uint32_t max, uint32_t value) {
    (void)min; (void)max; (void)value;
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_PROGRESS, "progress");
    if (!w) return NULL;
    w->draw = progress_draw;
w->layout_params.width = 200;
    w->layout_params.height = 24;
    w->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    w->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    return w;
}

static void spinner_draw(ui_widget_t* widget, void* canvas) {
    draw_rect(canvas, &widget->rect, widget->style.colors.bg);
}

ui_widget_t* ui_spinner_create(ui_context_t* ctx) {
    ui_widget_t* w = ui_widget_create(ctx, UI_WIDGET_SPINNER, "spinner");
    if (!w) return NULL;
    w->draw = spinner_draw;
    w->layout_params.width = 32;
    w->layout_params.height = 32;
    w->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    w->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    return w;
}

static void image_draw(ui_widget_t* widget, void* canvas) {
    draw_rect(canvas, &widget->rect, widget->style.colors.bg);
}

ui_widget_t* ui_image_create(ui_context_t* ctx, const void* image_data, uint16_t w, uint16_t h) {
    (void)image_data;
    ui_widget_t* widget = ui_widget_create(ctx, UI_WIDGET_IMAGE, "image");
    if (!widget) return NULL;
    widget->draw = image_draw;
    widget->layout_params.width = w;
    widget->layout_params.height = h;
    widget->layout_params.width_mode = UI_SIZE_MODE_FIXED;
    widget->layout_params.height_mode = UI_SIZE_MODE_FIXED;
    return widget;
}

void ui_render(ui_context_t* ctx) {
    if (!ctx || !ctx->root) return;
    
    // Render root and children
    ui_widget_t* root = ctx->root;
    if (root->draw) root->draw(root, NULL);
    
    for (uint32_t i = 0; i < root->child_count; i++) {
        ui_widget_t* child = root->children[i];
        if (child->visible && child->draw) {
            child->draw(child, NULL);
        }
    }
}

void ui_layout(ui_context_t* ctx) {
    if (!ctx || !ctx->root) return;
    
    if (ctx->root->layout_fn) {
        ctx->root->layout_fn(ctx->root);
    }
}

void ui_input_event(ui_context_t* ctx, const input_event_t* event) {
    if (!ctx || !event) return;
    
    // Handle focus navigation
    if (event->type == INPUT_EVENT_KEY_DOWN) {
        switch (event->key) {
            case INPUT_KEY_TAB:
                // Move focus to next widget
                break;
            case INPUT_KEY_ENTER:
                if (ctx->focused && ctx->focused->on_click) {
                    ctx->focused->on_click(ctx->focused, ctx->focused->user_data);
                }
                break;
            default:
                break;
        }
    }
}