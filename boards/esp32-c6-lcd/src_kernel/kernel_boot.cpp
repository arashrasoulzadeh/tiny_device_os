// ArdubotOS kernel boot wrapper for ESP32-C6.
//
// Arduino's app_main() calls setup()/loop() here. Apps are not drawn in
// this file: stdapps_install() registers the builtins compiled from
// apps/stdapps/, and app_start() runs one of them. The scheduler is
// kernel/scheduler_esp32.c (FreeRTOS tasks), not the sim fiber scheduler.
#include <Arduino.h>
#include <stdlib.h>
#include <time.h>

#if __has_include("device_config.h")
#include "device_config.h"
#endif

extern "C" {
#include "scheduler.h"
#include "os_time.h"
#include "host_stack.h"
#include "hal_display.h"
#include "app.h"
#include "app_types.h"
#include "sim_gpio.h"
#include "ardubot_keys.h"
#include "stdapps_register.h"
#include "os_clock.h"
#include "clock_service.h"
#include "sensor_service.h"
#include "hal_storage.h"
#include "littlefs_vfs.h"
}

static void install_board_clock(void) {
    char tz[16];
    int offset_min = 0;
    time_t compiled = 0;
#ifdef ARDUBOT_CLOCK_TZ_OFFSET_MIN
    offset_min = ARDUBOT_CLOCK_TZ_OFFSET_MIN;
#endif
#ifdef ARDUBOT_CLOCK_UNIX
    compiled = (time_t)ARDUBOT_CLOCK_UNIX;
#endif
    if (os_clock_tz_string(offset_min, tz, sizeof(tz)) == 0) {
        setenv("TZ", tz, 1);
        tzset();
    }

    hal_storage_t* flash = hal_storage_open("/dev/flash0", HAL_STORAGE_TYPE_FLASH);
    if (flash && hal_storage_init(flash) == 0) {
        hal_storage_info_t info;
        uint32_t bytes = 256 * 1024;
        if (hal_storage_get_info(flash, &info) == 0 && info.total_bytes >= 4096) {
            bytes = info.total_bytes - (info.total_bytes % 4096);
        }
        if (littlefs_mount(flash, 0, bytes, 4096, "/flash") != 0) {
            Serial.println("[kernel_boot] /flash mount failed");
        }
    } else {
        Serial.println("[kernel_boot] flash storage unavailable");
    }

    if (clock_service_start(time(NULL), compiled) != 0) {
        Serial.println("[kernel_boot] clock service failed");
    }
}

// Bridges the board's two real buttons (GPIO18/19, same wiring as
// device_config_esp32c6.yaml) into sim_gpio's key-state table. Apps in
// apps/stdapps/ bind keys through that table. UP is held as SIM_KEY_UP.
// A short SELECT press is SIM_KEY_ENTER; holding SELECT is SIM_KEY_ESCAPE
// (back to the launcher).
#define KERNEL_BOOT_BTN_UP_GPIO 18
#define KERNEL_BOOT_BTN_SELECT_GPIO 19
#define KERNEL_BOOT_LONG_PRESS_MS 700

static void poll_real_buttons(void) {
    static bool up_was_down = false;
    static bool sel_was_down = false;
    static uint32_t sel_down_ms = 0;
    static bool sel_long_fired = false;

    bool up_down = digitalRead(KERNEL_BOOT_BTN_UP_GPIO) == LOW;
    bool sel_down = digitalRead(KERNEL_BOOT_BTN_SELECT_GPIO) == LOW;

    if (up_down != up_was_down) {
        sim_gpio_handle_key(SIM_KEY_UP, up_down);
        up_was_down = up_down;
    }

    uint32_t now = millis();
    if (sel_down && !sel_was_down) {
        sel_was_down = true;
        sel_down_ms = now;
        sel_long_fired = false;
    } else if (sel_down && sel_was_down && !sel_long_fired &&
               (now - sel_down_ms) >= KERNEL_BOOT_LONG_PRESS_MS) {
        sel_long_fired = true;
        sim_gpio_handle_key(SIM_KEY_ESCAPE, true);
        sim_gpio_handle_key(SIM_KEY_ESCAPE, false);
    } else if (!sel_down && sel_was_down) {
        if (!sel_long_fired) {
            sim_gpio_handle_key(SIM_KEY_ENTER, true);
            sim_gpio_handle_key(SIM_KEY_ENTER, false);
        }
        sel_was_down = false;
    }
}

static task_tcb_t* g_demo_task_tcb;
static uint32_t g_demo_tick_count;

static void demo_task(void* arg) {
    (void)arg;
    for (;;) {
        g_demo_tick_count++;
        Serial.printf("[kernel_boot] demo_task tick=%lu\n",
                       (unsigned long)g_demo_tick_count);
        task_sleep(1000);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[kernel_boot] ArdubotOS real kernel starting (Phase 2 skeleton)");
    install_board_clock();
    sensor_service_load_builtin();

    pinMode(KERNEL_BOOT_BTN_UP_GPIO, INPUT_PULLUP);
    pinMode(KERNEL_BOOT_BTN_SELECT_GPIO, INPUT_PULLUP);
    sim_gpio_init();

    if (scheduler_init() != 0) {
        Serial.println("[kernel_boot] scheduler_init failed");
        return;
    }

    if (task_create("demo", demo_task, NULL, TASK_PRIO_NORMAL, HOST_STACK_MIN_BYTES,
                     &g_demo_task_tcb) != 0) {
        Serial.println("[kernel_boot] task_create failed");
        return;
    }

    if (scheduler_start() != 0) {
        Serial.println("[kernel_boot] scheduler_start failed");
        return;
    }

    Serial.println("[kernel_boot] scheduler started - boot banner printed");

    // Every builtin comes from apps/stdapps via stdapps_install().
    if (app_init() != 0 || stdapps_install() != 0) {
        Serial.println("[kernel_boot] stdapps_install failed");
        return;
    }
    const char* start = stdapps_start_name();
    if (app_start(start) != 0) {
        Serial.printf("[kernel_boot] app_start(%s) failed\n", start);
        return;
    }
    Serial.printf("[kernel_boot] %s started - entering loop()\n", start);
}

void loop() {
    poll_real_buttons();

    static uint32_t last_tick_ms = 0;
    uint32_t now_ms = millis();
    uint32_t dt = now_ms - last_tick_ms;

    /* Unlike sim_main.c's SDL loop (paced to ~1ms/iteration by its own
     * sim_time_sleep_ms(1)), Arduino's loop() here has no such pacing and
     * runs far faster than 1ms per iteration whenever there's little
     * work to do - dt is 0 on nearly every call. Forcing dt=1 and
     * ticking anyway (sim_main.c's fallback, copied here without this
     * difference in mind) made the scheduler tick roughly once per
     * loop() iteration instead of once per real millisecond, racing
     * "uptime" far ahead of wall-clock time (confirmed on hardware,
     * RISCV_TODO.md Phase 4: ~100x). Skip ticking entirely when no real
     * time has actually elapsed. */
    if (dt == 0) {
        return;
    }
    if (dt > 50) {
        dt = 50; /* clamp after any stall */
    }
    last_tick_ms = now_ms;

    time_set_now_us((time_us_t)now_ms * 1000);

    for (uint32_t t = 0; t < dt; t++) {
        scheduler_tick();
    }
    scheduler_step();
    timers_process();
}
