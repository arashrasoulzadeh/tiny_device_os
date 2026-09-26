#include "device_info.h"

#include "app.h"
#include "scheduler.h"
#include "ssd1306_model.h"

#include <string.h>

#ifndef ARDUBOT_VERSION_STR
#define ARDUBOT_VERSION_STR "0.1.0"
#endif

static const char* device_target_name(void) {
#if defined(ARDUBOT_SIM_SDL2)
    return "sim";
#elif defined(ARDUBOT_TARGET_ESP32)
    return "esp32";
#elif defined(ARDUBOT_TARGET_ESP8266)
    return "esp8266";
#elif defined(ARDUBOT_TARGET_AVR)
    return "avr";
#elif defined(ARDUBOT_TARGET_RP2040)
    return "rp2040";
#else
    return "unknown";
#endif
}

static const char* device_arch_name(void) {
#if defined(__AVR__)
    return "avr";
#elif defined(__xtensa__)
    return "xtensa";
#elif defined(__aarch64__)
    return "aarch64";
#elif defined(__arm__)
    return "arm";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

int device_info_query(device_info_t* out) {
    if (!out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    out->os_name = "ArdubotOS";
    out->os_version = ARDUBOT_VERSION_STR;
    out->target = device_target_name();
    out->arch = device_arch_name();
    out->display_w = SSD1306_WIDTH;
    out->display_h = SSD1306_HEIGHT;
    out->uptime_ms = scheduler_get_tick_count();

    app_t* apps[APP_MAX];
    size_t count = 0;
    if (app_list(apps, APP_MAX, &count) == 0) {
        out->installed_apps = (uint32_t)count;
    }

    return 0;
}
