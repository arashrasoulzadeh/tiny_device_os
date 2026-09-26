#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include "scheduler.h"
#include "syscall.h"
#include "app.h"
#include "vfs.h"
#include "stdlog.h"
#include "os_time.h"
#include "sim_gpio.h"
#include "hal_gpio.h"
#include "hal_display.h"
#include "hal_audio.h"
#include "hal_storage.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_uart.h"
#include "ssd1306_model.h"
#include "bmp280_model.h"
#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SIMPLIFIED MANIFEST CREATION
// ============================================================================

// Common capability presets
#define APP_CAPS_NONE           {}
#define APP_CAPS_BASIC          { CAP_DISPLAY_ACCESS, CAP_EVENT_ACCESS }
#define APP_CAPS_DISPLAY        { CAP_DISPLAY_ACCESS }
#define APP_CAPS_AUDIO          { CAP_AUDIO_ACCESS }
#define APP_CAPS_STORAGE        { CAP_STORAGE_ACCESS, CAP_FS_ACCESS }
#define APP_CAPS_NETWORK        { CAP_NET_ACCESS, CAP_WIFI_ACCESS }
#define APP_CAPS_GPIO           { CAP_GPIO_READ, CAP_GPIO_WRITE }
#define APP_CAPS_I2C            { CAP_I2C_ACCESS }
#define APP_CAPS_SPI            { CAP_SPI_ACCESS }
#define APP_CAPS_UART           { CAP_UART_ACCESS }
#define APP_CAPS_FULL           { CAP_DISPLAY_ACCESS, CAP_EVENT_ACCESS, CAP_GPIO_READ, CAP_GPIO_WRITE, \
                                  CAP_I2C_ACCESS, CAP_SPI_ACCESS, CAP_UART_ACCESS, CAP_AUDIO_ACCESS, \
                                  CAP_STORAGE_ACCESS, CAP_FS_ACCESS, CAP_NET_ACCESS, CAP_WIFI_ACCESS, \
                                  CAP_CONFIG_ACCESS, CAP_POWER_MGMT }

// Stack/heap size presets
#define APP_STACK_TINY      4096
#define APP_STACK_SMALL     8192
#define APP_STACK_MEDIUM    16384
#define APP_STACK_LARGE     32768
#define APP_STACK_HUGE      65536

#define APP_HEAP_TINY       8192
#define APP_HEAP_SMALL      32768
#define APP_HEAP_MEDIUM     65536
#define APP_HEAP_LARGE      131072
#define APP_HEAP_HUGE       262144

// App type presets
#define APP_TYPE_DEFAULT    APP_TYPE_USER
#define APP_TYPE_GAME_APP   APP_TYPE_GAME
#define APP_TYPE_TOOL_APP   APP_TYPE_TOOL
#define APP_TYPE_SYS_APP    APP_TYPE_SYSTEM

// Manifest builder function - call in app init or constructor
static inline app_manifest_t* app_manifest_create(const char* name, const char* version,
                                                  app_type_t type, uint32_t min_os_ver,
                                                  void (*entry)(void),
                                                  uint32_t stack_size, uint32_t heap_size,
                                                  const capability_t* caps, uint32_t cap_count,
                                                  const char* author, const char* description) {
    static app_manifest_t manifest = {0};
    manifest.type = type;
    manifest.min_os_version = min_os_ver;
    manifest.entry_point = (uintptr_t)entry;
    manifest.stack_size = stack_size > 0 ? stack_size : APP_STACK_SMALL;
    manifest.heap_size = heap_size > 0 ? heap_size : APP_HEAP_SMALL;
    
    if (name) strncpy(manifest.name, name, APP_NAME_MAX - 1);
    if (version) strncpy(manifest.version, version, 15);
    if (author) strncpy(manifest.author, author, 63);
    if (description) strncpy(manifest.description, description, 255);
    
    if (caps && cap_count > 0) {
        manifest.capability_count = (cap_count < 16) ? cap_count : 16;
        for (uint32_t i = 0; i < manifest.capability_count; i++) {
            manifest.capabilities[i] = caps[i];
        }
    }
    
    return &manifest;
}

// ============================================================================
// APP LIFECYCLE HELPERS
// ============================================================================

// App state callbacks
typedef struct {
    void (*on_init)(void);           // Called once at startup
    void (*on_start)(void);          // Called when app becomes active
    void (*on_stop)(void);           // Called when app stops
    void (*on_suspend)(void);        // Called when app suspended
    void (*on_resume)(void);         // Called when app resumes
    void (*on_loop)(void);           // Called every frame
    void (*on_render)(void);         // Called for rendering
    void (*on_event)(uint32_t event, void* data); // System events
    void (*on_cleanup)(void);        // Called on app exit
} app_lifecycle_t;

// Global lifecycle (set by app)
extern app_lifecycle_t g_app_lifecycle;

// Run app with lifecycle
int app_run_with_lifecycle(const app_lifecycle_t* lifecycle);

// ============================================================================
// DISPLAY HELPERS
// ============================================================================

// Simple display wrapper using the build-time panel size
typedef struct {
    bool initialized;
    uint16_t width;
    uint16_t height;
} app_display_t;

static inline int app_display_init(app_display_t* disp, const char* dev_path) {
    (void)dev_path;
    if (!disp) return -1;
    ssd1306_model_register();
    disp->width = APP_DISPLAY_WIDTH;
    disp->height = APP_DISPLAY_HEIGHT;
    disp->initialized = true;
    return 0;
}

static inline void app_display_deinit(app_display_t* disp) {
    (void)disp;
}

static inline void app_display_clear(app_display_t* disp) {
    if (disp && disp->initialized) ssd1306_model_clear();
}

static inline void app_display_text(app_display_t* disp, int x, int y, const char* text) {
    if (disp && disp->initialized) ssd1306_model_draw_text(x, y, text);
}

static inline void app_display_flush(app_display_t* disp) {
    if (disp && disp->initialized) ssd1306_model_render();
}

static inline void app_display_rect(app_display_t* disp, int x, int y, int w, int h, bool fill) {
    (void)disp; (void)x; (void)y; (void)w; (void)h; (void)fill;
    // Not directly supported, use hal_display functions if needed
}

static inline void app_display_set_rotation(app_display_t* disp, uint8_t rot) {
    (void)disp; (void)rot;
}

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
    
    // Open HAL GPIO
    char path[32];
    snprintf(path, sizeof(path), "/dev/gpio%d", btn->pin);
    hal_gpio_t* gpio = hal_gpio_open(path, HAL_GPIO_MODE_INPUT_PULLUP);
    if (!gpio) return -1;
    
    // Map key to pin
    sim_gpio_set_key_mapping(btn->key, btn->pin, true);
    
    // Register callback with proper trigger
    if (btn->on_press || btn->on_release) {
        sim_gpio_register_hal_gpio_with_trigger(btn->pin, 
            (hal_gpio_callback_t)(btn->trigger == HAL_GPIO_IRQ_FALLING ? btn->on_release : btn->on_press),
            btn->arg, btn->trigger);
    }
    
    hal_gpio_set_irq(gpio, btn->trigger, NULL, NULL);
    hal_gpio_enable_irq(gpio);
    
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

// ============================================================================
// TIMING/FPS HELPERS
// ============================================================================

typedef struct {
    uint32_t target_fps;
    uint32_t frame_time_ms;
    uint32_t last_frame_ms;
    uint32_t frame_count;
    uint32_t delta_ms;
} app_timer_t;

static inline void app_timer_init(app_timer_t* timer, uint32_t fps) {
    timer->target_fps = fps;
    timer->frame_time_ms = (fps > 0) ? (1000 / fps) : 0;
    timer->last_frame_ms = time_now_ms();
    timer->frame_count = 0;
    timer->delta_ms = 0;
}

static inline uint32_t app_timer_get_tick(void) {
    return time_now_ms();
}

static inline bool app_timer_should_frame(app_timer_t* timer) {
    uint32_t now = app_timer_get_tick();
    if (now - timer->last_frame_ms >= timer->frame_time_ms) {
        timer->delta_ms = now - timer->last_frame_ms;
        timer->last_frame_ms = now;
        timer->frame_count++;
        return true;
    }
    return false;
}

static inline void app_timer_sleep_remaining(app_timer_t* timer) {
    uint32_t now = app_timer_get_tick();
    uint32_t elapsed = now - timer->last_frame_ms;
    if (elapsed < timer->frame_time_ms) {
        time_sleep_ms(timer->frame_time_ms - elapsed);
    }
}

// ============================================================================
// LOGGING HELPERS
// ============================================================================

// Internal logging function using stdlog_vlog correctly
static inline void _app_log(log_level_t level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    stdlog_vlog(level, __FILE__, __LINE__, __func__, fmt, args);
    va_end(args);
}

// Use standard logging with app name prefix
#define APP_LOG_DEBUG(fmt, ...)   _app_log(LOG_LEVEL_DEBUG, "[APP] " fmt, ##__VA_ARGS__)
#define APP_LOG_INFO(fmt, ...)    _app_log(LOG_LEVEL_INFO, "[APP] " fmt, ##__VA_ARGS__)
#define APP_LOG_WARN(fmt, ...)    _app_log(LOG_LEVEL_WARN, "[APP] " fmt, ##__VA_ARGS__)
#define APP_LOG_ERROR(fmt, ...)   _app_log(LOG_LEVEL_ERROR, "[APP] " fmt, ##__VA_ARGS__)

// Short aliases
#define APP_DEBUG(fmt, ...)       _app_log(LOG_LEVEL_DEBUG, "[APP] " fmt, ##__VA_ARGS__)
#define APP_INFO(fmt, ...)        _app_log(LOG_LEVEL_INFO, "[APP] " fmt, ##__VA_ARGS__)
#define APP_WARN(fmt, ...)        _app_log(LOG_LEVEL_WARN, "[APP] " fmt, ##__VA_ARGS__)
#define APP_ERROR(fmt, ...)       _app_log(LOG_LEVEL_ERROR, "[APP] " fmt, ##__VA_ARGS__)

// ============================================================================
// CONFIG/SETTINGS HELPERS
// ============================================================================

static inline int app_config_set_str(const char* key, const char* value) {
    extern int config_set_string(const char*, const char*);
    return config_set_string(key, value);
}

static inline int app_config_get_str(const char* key, char* value, size_t max_len) {
    extern int config_get_string(const char*, char*, size_t);
    return config_get_string(key, value, max_len);
}

static inline int app_config_set_int(const char* key, int32_t value) {
    extern int config_set_int(const char*, int32_t);
    return config_set_int(key, value);
}

static inline int app_config_get_int(const char* key, int32_t* value) {
    extern int config_get_int(const char*, int32_t*);
    return config_get_int(key, value);
}

static inline int app_config_set_bool(const char* key, bool value) {
    extern int config_set_bool(const char*, bool);
    return config_set_bool(key, value);
}

static inline int app_config_get_bool(const char* key, bool* value) {
    extern int config_get_bool(const char*, bool*);
    return config_get_bool(key, value);
}

static inline void app_config_save(void) {
    extern int config_flush(void);
    config_flush();
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

// ============================================================================
// AUDIO HELPERS
// ============================================================================

typedef struct {
    hal_audio_t* handle;
    uint32_t sample_rate;
    uint16_t channels;
} app_audio_t;

static inline int app_audio_init(app_audio_t* audio, uint32_t sample_rate, uint16_t channels) {
    if (!audio) return -1;
    hal_audio_config_t cfg = {
        .sample_rate = sample_rate,
        .channels = channels,
        .format = HAL_AUDIO_FORMAT_PCM_S16_LE,
        .buffer_frames = 512,
        .period_frames = 256,
        .output = true,
        .input = false
    };
    audio->handle = hal_audio_open("/dev/audio0", &cfg);
    if (!audio->handle) return -1;
    audio->sample_rate = sample_rate;
    audio->channels = channels;
    return hal_audio_start(audio->handle);
}

static inline void app_audio_deinit(app_audio_t* audio) {
    if (audio && audio->handle) {
        hal_audio_stop(audio->handle);
        hal_audio_close(audio->handle);
        audio->handle = NULL;
    }
}

static inline int app_audio_play(app_audio_t* audio, const int16_t* samples, size_t count) {
    if (!audio || !audio->handle || !samples) return -1;
    return hal_audio_write(audio->handle, samples, count);
}

// ============================================================================
// SIMPLE APP TEMPLATE
// ============================================================================

// Simple app entry point template
// Usage: 
//   app_manifest_t* my_manifest = app_manifest_create(...);
//   int app_main(void) { ... init ... while(1) { ... loop ... } }
#define APP_ENTRY_POINT(name) \
    app_manifest_t* name##_manifest = NULL; \
    __attribute__((constructor)) \
    static void name##_manifest_init(void) { \
        extern app_manifest_t* app_manifest_create(...); /* placeholder */ \
    } \
    int name##_entry(void)

#ifdef __cplusplus
}
#endif