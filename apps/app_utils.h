#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "scheduler.h"
#include "syscall.h"
#include "app.h"
#include "sim_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// FPS timing helpers
typedef enum {
    APP_FPS_1 = 1,
    APP_FPS_24 = 24,
    APP_FPS_30 = 30,
    APP_FPS_60 = 60,
    APP_FPS_120 = 120,
} app_fps_t;

// Sleep for the duration of one frame at the given FPS
static inline void app_sleep_fps(app_fps_t fps) {
    if (fps == 0) return;
    uint32_t ticks_per_frame = 1000 / fps; // Convert to milliseconds
    task_sleep(ticks_per_frame);
}

// Sleep for specific milliseconds
static inline void app_sleep_ms(uint32_t ms) {
    task_sleep(ms);
}

// Sleep for specific microseconds (if supported)
static inline void app_sleep_us(uint32_t us) {
    task_sleep(us / 1000); // Convert to ms
}

// App loop helper - runs a callback at specified FPS
typedef void (*app_loop_fn_t)(void* arg);

typedef struct {
    app_loop_fn_t fn;
    void* arg;
    app_fps_t target_fps;
    uint32_t frame_count;
    uint32_t last_tick;
} app_loop_t;

// Initialize app loop
static inline void app_loop_init(app_loop_t* loop, app_loop_fn_t fn, void* arg, app_fps_t fps) {
    loop->fn = fn;
    loop->arg = arg;
    loop->target_fps = fps;
    loop->frame_count = 0;
    loop->last_tick = 0;
}

// Run one iteration of the app loop (call in main loop)
static inline void app_loop_run(app_loop_t* loop) {
    if (loop->fn) {
        loop->fn(loop->arg);
        loop->frame_count++;
    }
    if (loop->target_fps > 0) {
        app_sleep_fps(loop->target_fps);
    }
}

// Get current frame count
static inline uint32_t app_loop_get_frame_count(const app_loop_t* loop) {
    return loop->frame_count;
}

// Get elapsed time in milliseconds since loop start
static inline uint32_t app_loop_get_elapsed_ms(const app_loop_t* loop) {
    return loop->frame_count * (1000 / loop->target_fps);
}

// GPIO helper macros for easier pin setup
#define APP_GPIO_INPUT(pin) sim_gpio_register(pin, false)
#define APP_GPIO_OUTPUT(pin, initial) sim_gpio_register(pin, true)

// Key mapping helper
static inline void app_gpio_map_key(int pin, sim_key_t key) {
    sim_gpio_set_key_mapping(key, pin, true);
}

// Button handler helper - use wrapper to match hal_gpio_callback_t signature
typedef void (*app_button_cb_t)(int pin, void* arg);

static inline void app_gpio_set_button_handler(int pin, void (*cb)(int, void*), void* arg) {
    sim_gpio_register_hal_gpio(pin, cb, arg);
}

// App initialization helpers
typedef struct {
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    uint32_t stack_size;
    uint32_t heap_size;
    capability_t* capabilities;
    uint32_t capability_count;
    void (*entry)(void);
} app_info_t;

static inline int app_register_builtin(const app_info_t* info) {
    if (!info || !info->name || !info->entry) return -1;
    
    app_manifest_t manifest = {0};
    
    manifest.type = APP_TYPE_USER;
    manifest.min_os_version = 1;
    manifest.entry_point = (uintptr_t)info->entry;
    manifest.stack_size = info->stack_size > 0 ? info->stack_size : 8192;
    manifest.heap_size = info->heap_size > 0 ? info->heap_size : 32768;
    
    if (info->name) strncpy(manifest.name, info->name, APP_NAME_MAX - 1);
    if (info->version) strncpy(manifest.version, info->version, 15);
    if (info->author) strncpy(manifest.author, info->author, 63);
    if (info->description) strncpy(manifest.description, info->description, 255);
    
    if (info->capabilities && info->capability_count > 0) {
        manifest.capability_count = info->capability_count;
        // Copy capabilities up to max
        uint32_t count = info->capability_count < 16 ? info->capability_count : 16;
        for (uint32_t i = 0; i < count; i++) {
            manifest.capabilities[i] = info->capabilities[i];
        }
    }
    
    return app_install_manifest(&manifest, info->name);
}

// Logging macros
#define APP_LOG_DEBUG(fmt, ...) stdlog_debug(fmt, ##__VA_ARGS__)
#define APP_LOG_INFO(fmt, ...) stdlog_info(fmt, ##__VA_ARGS__)
#define APP_LOG_WARN(fmt, ...) stdlog_warn(fmt, ##__VA_ARGS__)
#define APP_LOG_ERROR(fmt, ...) stdlog_error(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif