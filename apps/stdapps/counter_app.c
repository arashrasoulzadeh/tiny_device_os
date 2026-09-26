#include "app_framework.h"
#include "ssd1306_model.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void counter_app_entry(void);

/* --- App State --- */
static int32_t g_counter = 0;
static int32_t g_last_counter = -1;
static app_display_t g_display;
static app_timer_t g_timer;

/* --- Button Handlers --- */
static void on_inc_press(int pin, void* arg) {
    (void)pin;
    (void)arg;
    g_counter++;
    APP_INFO("Counter incremented to %d", g_counter);
}

static void on_dec_press(int pin, void* arg) {
    (void)pin;
    (void)arg;
    g_counter--;
    APP_INFO("Counter decremented to %d", g_counter);
}

/* --- Button Config --- */
static app_button_t g_buttons[] = {
    APP_BUTTON(1, SIM_KEY_1, on_inc_press, NULL, NULL),
    APP_BUTTON(2, SIM_KEY_2, on_dec_press, NULL, NULL),
};
static app_buttons_t g_button_set = {
    .buttons = g_buttons,
    .count = sizeof(g_buttons) / sizeof(g_buttons[0]),
};

/* --- Display Rendering --- */
static void draw_counter(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Count: %d", g_counter);

    app_display_clear(&g_display);
    app_display_text(&g_display, 0, 0, "Counter App");
    app_display_text(&g_display, 0, 16, buf);
    app_display_text(&g_display, 0, 32, "Press 1/2 to change");
    app_display_flush(&g_display);
}

/* --- Lifecycle --- */
static void counter_app_init(void) {
    if (app_display_init(&g_display, "/dev/display0") != 0) {
        APP_ERROR("Display init failed");
        return;
    }
    if (app_buttons_init(&g_button_set) != 0) {
        APP_ERROR("Button init failed");
    }
    app_timer_init(&g_timer, 30);
    draw_counter();
    APP_INFO("Counter App ready");
}

static void counter_app_loop(void) {
    if (g_counter != g_last_counter) {
        draw_counter();
        g_last_counter = g_counter;
    }
    if (app_timer_should_frame(&g_timer)) {
        app_timer_sleep_remaining(&g_timer);
    }
}

static void counter_app_cleanup(void) {
    app_display_deinit(&g_display);
    APP_INFO("Counter App cleaned up");
}

static app_lifecycle_t s_lifecycle = {
    .on_init = counter_app_init,
    .on_loop = counter_app_loop,
    .on_cleanup = counter_app_cleanup,
};

static app_manifest_t* create_manifest(void) {
    static capability_t caps[] = APP_CAPS_BASIC;
    return app_manifest_create("counter", "2.0.0", APP_TYPE_USER, 1, counter_app_entry,
                               APP_STACK_SMALL, APP_HEAP_SMALL, caps,
                               (uint32_t)(sizeof(caps) / sizeof(caps[0])), "ArdubotOS",
                               "Simple counter demo using app_framework");
}

static void counter_app_entry(void) {
    if (s_lifecycle.on_init) {
        s_lifecycle.on_init();
    }
    while (1) {
        if (s_lifecycle.on_loop) {
            s_lifecycle.on_loop();
        }
        task_sleep(16);
    }
}

app_manifest_t* counter_app_manifest = NULL;

__attribute__((constructor)) static void export_manifest(void) {
    counter_app_manifest = create_manifest();
}

void counter_app_tick(void) {
    if (s_lifecycle.on_loop) {
        s_lifecycle.on_loop();
    }
}
