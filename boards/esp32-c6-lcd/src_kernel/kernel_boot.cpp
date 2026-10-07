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
#if __has_include("device_secrets.h")
#include "device_secrets.h"
#endif

extern "C" {
#include "scheduler.h"
#include "os_time.h"
#include "host_stack.h"
#include "power.h"
#include "hal_power.h"
#include "hal_display.h"
#include "app.h"
#include "app_types.h"
#include "sim_gpio.h"
#include "ardubot_keys.h"
#include "stdapps_register.h"
#include "os_clock.h"
#include "os_boot.h"
#include "sensor_service.h"
#if defined(ARDUBOT_LINK_FORWARD) && ARDUBOT_LINK_FORWARD && defined(ARDUBOT_LINK_HAS_KEY) && \
    ARDUBOT_LINK_HAS_KEY
#include "link_bridge.h"
#include "tty_service.h"
#include <esp_system.h>
#endif
#include "hal_storage.h"
#include "littlefs_vfs.h"
}

static void install_board_tz(void) {
    char tz[16];
    int offset_min = 0;
#ifdef ARDUBOT_CLOCK_TZ_OFFSET_MIN
    offset_min = ARDUBOT_CLOCK_TZ_OFFSET_MIN;
#endif
    if (os_clock_tz_string(offset_min, tz, sizeof(tz)) == 0) {
        setenv("TZ", tz, 1);
        tzset();
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

static hal_power_t* g_board_power;

extern "C" {

static int board_mount_storage(void) {
    hal_storage_t* flash = hal_storage_open("/dev/flash0", HAL_STORAGE_TYPE_FLASH);
    if (!flash || hal_storage_init(flash) != 0) {
        Serial.println("[kernel_boot] flash storage unavailable");
        return -1;
    }
    hal_storage_info_t info;
    uint32_t bytes = 256 * 1024;
    if (hal_storage_get_info(flash, &info) == 0 && info.total_bytes >= 4096) {
        bytes = info.total_bytes - (info.total_bytes % 4096);
    }
    if (littlefs_mount(flash, 0, bytes, 4096, "/flash") != 0) {
        Serial.println("[kernel_boot] /flash mount failed");
        return -1;
    }
    return 0;
}

static int board_set_freq(uint32_t mhz) {
    int rc = -1;
    /* ESP32-C6 tops out at 160 MHz. The classic ESP32 table still lists 240. */
    if (mhz > 160) {
        mhz = 160;
    }
    if (g_board_power) {
        rc = hal_power_set_cpu_freq(g_board_power, mhz);
    }
    if (power_set_cpu_freq(mhz) == 0) {
        rc = 0;
    }
    return rc;
}

static int board_set_brightness(uint8_t cap) {
    return hal_display_set_backlight_cap(cap);
}

static int board_read_temp(int32_t* temp_c) {
    int32_t decideg = 0;
    if (!temp_c) {
        return -1;
    }
    /* sensor_service already owns the on-chip sensor when "temp" is configured. */
    if (sensor_get("temp", &decideg) == 0) {
        *temp_c = decideg / 10;
        return 0;
    }
    if (g_board_power && hal_power_get_die_temp_c(g_board_power, temp_c) == 0) {
        return 0;
    }
    return -1;
}

} /* extern "C" */

static void bind_power_governor(void) {
    static const uint32_t c6_freqs[] = {160, 80, 40, 20, 10};
    power_init();
    g_board_power = hal_power_open("/dev/power0");
    if (g_board_power) {
        (void)hal_power_init(g_board_power);
    }
    (void)power_set_available_freqs(c6_freqs, 5);
    power_governor_set_actuators(board_set_freq, board_set_brightness, board_read_temp);
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
    install_board_tz();

    pinMode(KERNEL_BOOT_BTN_UP_GPIO, INPUT_PULLUP);
    pinMode(KERNEL_BOOT_BTN_SELECT_GPIO, INPUT_PULLUP);
    sim_gpio_init();

    if (scheduler_init() != 0) {
        Serial.println("[kernel_boot] scheduler_init failed");
        return;
    }
    bind_power_governor();

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
    // os_boot_load then mounts /flash, starts services, and runs on_load.
    if (app_init() != 0 || stdapps_install() != 0) {
        Serial.println("[kernel_boot] stdapps_install failed");
        return;
    }
    {
        os_boot_args_t args;
        time_t compiled = 0;
#ifdef ARDUBOT_CLOCK_UNIX
        compiled = (time_t)ARDUBOT_CLOCK_UNIX;
#endif
        args.mount_storage = board_mount_storage;
        args.clock_now = time(NULL);
        args.clock_compiled = compiled;
        if (os_boot_load(&args) != 0) {
            Serial.println("[kernel_boot] os load failed");
            return;
        }
    }
    const char* start = stdapps_start_name();
    if (app_start(start) != 0) {
        Serial.printf("[kernel_boot] app_start(%s) failed\n", start);
        return;
    }
    Serial.printf("[kernel_boot] %s started - entering loop()\n", start);
}

static void poll_host_link(void) {
#if defined(ARDUBOT_LINK_FORWARD) && ARDUBOT_LINK_FORWARD && defined(ARDUBOT_LINK_HAS_KEY) && \
    ARDUBOT_LINK_HAS_KEY
    static int ready = 0;
    uint8_t rx[64];
    uint8_t tx[1024];
    size_t n = 0;
    int out;
    if (!ready) {
        static const uint8_t key[16] = ARDUBOT_LINK_KEY;
        link_bridge_init(key, (uint8_t)ARDUBOT_LINK_KEY_ID);
        tty_service_set_reboot(esp_restart);
        ready = 1;
    }
    while (Serial.available() > 0 && n < sizeof(rx)) {
        int byte = Serial.read();
        if (byte < 0) {
            break;
        }
        rx[n++] = (uint8_t)byte;
    }
    if (n > 0) {
        out = link_bridge_rx(rx, n, tx, sizeof(tx));
        if (out > 0) {
            Serial.write(tx, (size_t)out);
        }
    }
    out = link_bridge_poll((uint32_t)millis(), tx, sizeof(tx));
    if (out > 0) {
        Serial.write(tx, (size_t)out);
    }
#endif
}

void loop() {
    poll_host_link();
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
        /* A bare return spins loopTask and starves the idle task, so a CPU
         * reading flips between 0 and 100. Sleep one tick instead. */
        vTaskDelay(1);
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
