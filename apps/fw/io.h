#pragma once

/* IO sub-framework: keys, the settings store, and files. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ardubot_keys.h"
#include "config_store.h"
#include "hal_gpio.h"
#include "sim_gpio.h"
#include "vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// BUTTON/INPUT HELPERS
// ============================================================================

// Button configuration
typedef struct {
    int pin;
    sim_key_t key;
    hal_gpio_irq_t trigger;
    void (*on_press)(int pin, void* arg);
    void (*on_release)(int pin, void* arg);
    void* arg;
} app_button_t;

static inline int app_button_init(const app_button_t* btn) {
    if (!btn) return -1;

    // Register sim GPIO
    sim_gpio_register(btn->pin, false);

#if !defined(ARDUBOT_TARGET_ESP32)
    /* btn->pin is a virtual slot index (apps/app_kit.c's g_next_pin,
     * starting at APP_KIT_PIN_BASE=20), meaningful only as a key into
     * sim_gpio's own in-memory table below - it has no relationship to
     * a real GPIO pin number. hal_gpio_open()/set_irq() here always
     * passed a NULL callback anyway (see below), so this path was
     * already functionally inert even on sim/host builds - it's real
     * hardware's gpio_config() that can't tell "virtual slot 21" from
     * "real GPIO21" (this board's ST7789 reset pin), which hung/faulted
     * by reconfiguring a pin another driver already owns (confirmed on
     * hardware, RISCV_TODO.md Phase 4). Skipped entirely on ESP32 - the
     * real key-press path is sim_gpio_handle_key(), driven by actual
     * button polling in kernel_boot.cpp, not this. */
    char path[32];
    snprintf(path, sizeof(path), "/dev/gpio%d", btn->pin);
    hal_gpio_t* gpio = hal_gpio_open(path, HAL_GPIO_MODE_INPUT_PULLUP);
    if (!gpio) return -1;
#endif

    // Map key to pin
    sim_gpio_set_key_mapping(btn->key, btn->pin, true);

    // Register callback with proper trigger
    if (btn->on_press || btn->on_release) {
        sim_gpio_register_hal_gpio_with_trigger(btn->pin,
            (hal_gpio_callback_t)(btn->trigger == HAL_GPIO_IRQ_FALLING ? btn->on_release : btn->on_press),
            btn->arg, btn->trigger);
    }

#if !defined(ARDUBOT_TARGET_ESP32)
    hal_gpio_set_irq(gpio, btn->trigger, NULL, NULL);
    hal_gpio_enable_irq(gpio);
#endif

    return 0;
}

// Multi-button helper
typedef struct {
    app_button_t* buttons;
    uint32_t count;
} app_buttons_t;

static inline int app_buttons_init(app_buttons_t* btns) {
    if (!btns || !btns->buttons) return -1;
    for (uint32_t i = 0; i < btns->count; i++) {
        if (app_button_init(&btns->buttons[i]) != 0) return -1;
    }
    return 0;
}

// Quick button setup macros
#define APP_BUTTON(pin_num, key_code, press_fn, release_fn, user_arg) \
    { .pin = pin_num, .key = key_code, .trigger = HAL_GPIO_IRQ_RISING, \
      .on_press = press_fn, .on_release = release_fn, .arg = user_arg }

// Used by app_ui_bind_key()'s trampoline, which can't tell press from
// release apart - HAL_GPIO_IRQ_BOTH fired it on both edges of one tap,
// doubling every nav/select action (e.g. one arrow press moving the
// selection twice). Press-only, matching APP_BUTTON above.
#define APP_KEY_BUTTON(pin_num, key_code, press_fn, release_fn, user_arg) \
    { .pin = pin_num, .key = key_code, .trigger = HAL_GPIO_IRQ_RISING, \
      .on_press = press_fn, .on_release = release_fn, .arg = user_arg }

// ============================================================================
// CONFIG/SETTINGS HELPERS - global store (for built-in settings)
// ============================================================================

static config_store_t* g_config_store = NULL;

static inline int app_config_set_str(const char* key, const char* value) {
    if (!g_config_store) return -1;
    return config_set_string(g_config_store, key, value);
}

static inline const char* app_config_get_str(const char* key, const char* def) {
    if (!g_config_store) return def;
    return config_get_string(g_config_store, key, def);
}

static inline int app_config_set_int(const char* key, int32_t value) {
    if (!g_config_store) return -1;
    return config_set_int(g_config_store, key, value);
}

static inline int app_config_get_int(const char* key, int32_t def) {
    if (!g_config_store) return def;
    return config_get_int(g_config_store, key, def);
}

static inline int app_config_set_bool(const char* key, bool value) {
    if (!g_config_store) return -1;
    return config_set_bool(g_config_store, key, value);
}

static inline bool app_config_get_bool(const char* key, bool def) {
    if (!g_config_store) return def;
    return config_get_bool(g_config_store, key, def);
}

static inline void app_config_init(config_store_t* store) {
    g_config_store = store;
}

static inline void app_config_save(void) {
    if (g_config_store) config_flush(g_config_store);
}

// ============================================================================
// STORAGE/FILE HELPERS
// ============================================================================

typedef vfs_file_t app_file_t;

static inline int app_file_open(app_file_t** file, const char* path, const char* mode) {
    vfs_mode_t vfs_mode = 0;
    if (strchr(mode, 'r')) vfs_mode |= VFS_MODE_READ;
    if (strchr(mode, 'w')) vfs_mode |= VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC;
    if (strchr(mode, 'a')) vfs_mode |= VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_APPEND;
    if (strchr(mode, '+')) vfs_mode |= VFS_MODE_READ | VFS_MODE_WRITE;
    return vfs_open(path, vfs_mode, file);
}

static inline int app_file_close(app_file_t* file) {
    return vfs_close(file);
}

static inline ssize_t app_file_read(app_file_t* file, void* buf, size_t count) {
    return vfs_read(file, buf, count);
}

static inline ssize_t app_file_write(app_file_t* file, const void* buf, size_t count) {
    return vfs_write(file, buf, count);
}

static inline int app_file_puts(app_file_t* file, const char* str) {
    return vfs_write(file, str, strlen(str));
}

static inline int app_file_gets(app_file_t* file, char* buf, size_t max) {
    size_t i = 0;
    char c;
    while (i < max - 1 && vfs_read(file, &c, 1) == 1) {
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return (i > 0) ? (int)i : -1;
}

static inline bool app_file_exists(const char* path) {
    vfs_stat_t st;
    return vfs_stat(path, &st) == 0;
}

static inline int app_file_remove(const char* path) {
    return vfs_unlink(path);
}

static inline int app_dir_create(const char* path) {
    return vfs_mkdir(path, 0755);
}

#ifdef __cplusplus
}
#endif
