#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct hal_display hal_display_t;

typedef struct {
    bool initialized;
    uint16_t width;
    uint16_t height;
} app_display_t;

typedef struct {
    uint32_t target_fps;
    uint32_t frame_time_ms;
    uint32_t last_frame_ms;
    uint32_t frame_count;
    uint32_t delta_ms;
} app_timer_t;

// UI configuration shared between app_kit and app_ui
typedef enum {
    APP_UI_MODE_UI = 0,
    APP_UI_MODE_GAME = 1,
} app_ui_mode_t;

typedef struct {
    app_ui_mode_t mode;
    char title[64];
    char help_text[128];
    bool show_top_bar;
    bool show_help_bar;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
    int text_scale;
} app_ui_config_t;

// Common capability presets (capability_t itself comes from app.h; these are
// plain array initializers, so no include is needed at definition time).
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

#if defined(ARDUBOT_PIO) || defined(ARDUBOT_TARGET_ESP8266)
#undef APP_STACK_TINY
#undef APP_STACK_SMALL
#undef APP_STACK_MEDIUM
#undef APP_HEAP_TINY
#undef APP_HEAP_SMALL
#define APP_STACK_TINY   1536
#define APP_STACK_SMALL  3072
#define APP_STACK_MEDIUM 4096
#define APP_HEAP_TINY    2048
#define APP_HEAP_SMALL   4096
#endif

// App type presets (APP_TYPE_* enum values come from app.h)
#define APP_TYPE_DEFAULT    APP_TYPE_USER
#define APP_TYPE_GAME_APP   APP_TYPE_GAME
#define APP_TYPE_TOOL_APP   APP_TYPE_TOOL
#define APP_TYPE_SYS_APP    APP_TYPE_SYSTEM
