// ArdubotOS kernel boot wrapper for ESP32-C6 (RISCV_TODO.md Phase 2).
//
// This is NOT boards/esp32-c6-lcd/src/main.cpp (the standalone Arduino
// sketch, left untouched as the proven fallback) - it links the REAL
// kernel/ sources against the Arduino framework's own startup/linking,
// which is already proven to boot on this board (see platformio.ini's
// [env:esp32-c6] / main.cpp). Arduino's app_main() supplies the startup
// code and calls setup()/loop() here, same contract as main.cpp, but
// setup()/loop() now drive the real kernel scheduler instead of
// reimplementing app logic by hand.
//
// Scope for Phase 2: no apps/stdapps yet, no display - just prove that
// scheduler_init()/task_create()/scheduler_start() link and actually run
// a task on real hardware. Backed by kernel/scheduler_esp32.c (real
// FreeRTOS tasks), not kernel/scheduler.c's cooperative fiber model used
// by sim - see RISCV_TODO.md Phase 2 for why the fiber model doesn't
// work under FreeRTOS's hardware stack-pointer guard on this chip.
#include <Arduino.h>

extern "C" {
#include "scheduler.h"
#include "os_time.h"
#include "host_stack.h"
#include "hal_display.h"
#include "app.h"
#include "app_types.h"
#include "sim_gpio.h"
#include "ardubot_keys.h"
}

// RISCV_TODO.md Phase 4/5: boots one real app through the real app
// framework (apps/app_kit.c's app_kit_run -> on_init/on_frame loop), not
// a hand-drawn test pattern. Both manifests are populated by APP_DEFINE's
// constructor-attribute registration before setup() runs - same
// extern-declare-and-use pattern sim/sim_main.c already relies on. Only
// one is actually app_start()'d (see setup() below) - the other stays
// linked in and ready to swap to.
extern app_manifest_t* pomodoro_app_manifest;
extern app_manifest_t* taskmgr_app_manifest;

// Bridges the board's two real buttons (GPIO18/19, same wiring as
// device_config_esp32c6.yaml and boards/esp32-c6-lcd/src/main.cpp's
// poll_buttons()) into sim_gpio's key-state table - apps/app_ui.c's key
// bindings read key state via sim_gpio_read()/react to
// sim_gpio_handle_key() edges, not via a real GPIO ISR callback (see
// RISCV_TODO.md Phase 4 for why). UP -> SIM_KEY_UP, SELECT -> SIM_KEY_ENTER
// (taskmgr_app.c binds UP to scroll; SELECT is unused there).
#define KERNEL_BOOT_BTN_UP_GPIO 18
#define KERNEL_BOOT_BTN_SELECT_GPIO 19

static void poll_real_buttons(void) {
    static bool up_was_down = false;
    static bool sel_was_down = false;

    bool up_down = digitalRead(KERNEL_BOOT_BTN_UP_GPIO) == LOW;
    bool sel_down = digitalRead(KERNEL_BOOT_BTN_SELECT_GPIO) == LOW;

    if (up_down != up_was_down) {
        sim_gpio_handle_key(SIM_KEY_UP, up_down);
        up_was_down = up_down;
    }
    if (sel_down != sel_was_down) {
        sim_gpio_handle_key(SIM_KEY_ENTER, sel_down);
        sel_was_down = sel_down;
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

    // RISCV_TODO.md Phase 4: boot one real app for real, through the real
    // app framework - this is the actual proof (not Phase 3's standalone
    // hal_display_* test pattern, which would otherwise double-init the
    // display driver alongside app_kit_run's own app_display_init()).
    if (!taskmgr_app_manifest) {
        Serial.println("[kernel_boot] taskmgr_app_manifest not registered (APP_DEFINE "
                        "constructor didn't run?)");
        return;
    }
    if (app_install_manifest(taskmgr_app_manifest, "taskmgr") != 0) {
        Serial.println("[kernel_boot] app_install_manifest(taskmgr) failed");
        return;
    }
    if (app_start("taskmgr") != 0) {
        Serial.println("[kernel_boot] app_start(taskmgr) failed");
        return;
    }
    Serial.println("[kernel_boot] taskmgr app started - entering loop()");
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
