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
}

// RISCV_TODO.md Phase 3: real ST7789 HAL driver smoke test. Draws a
// fixed test pattern once at boot through hal_display_* (not through
// apps/app_framework.h - wiring that up is Phase 4) to visually confirm
// the real HAL driver (hal/arch/esp32/hal_display_esp32_arduino.cpp)
// actually pushes pixels to the physical panel with correct
// orientation/colors.
static void draw_phase3_test_pattern(hal_display_t* d) {
    uint16_t w, h;
    hal_display_get_size(d, &w, &h);
    hal_display_fill_rect(d, 0, 0, w, h, 0x0000);                          // black bg
    hal_display_fill_rect(d, 0, 0, w / 3, h, 0xF800);                      // red
    hal_display_fill_rect(d, w / 3, 0, w / 3, h, 0x07E0);                  // green
    hal_display_fill_rect(d, 2 * (w / 3), 0, w - 2 * (w / 3), h, 0x001F);  // blue
    hal_display_draw_rect(d, 2, 2, w - 4, h - 4, 0xFFFF);                  // white border
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

    hal_display_t* display = hal_display_open("lcd0", NULL);
    if (!display || hal_display_init(display) != 0) {
        Serial.println("[kernel_boot] hal_display_init failed");
    } else {
        Serial.println("[kernel_boot] hal_display_init ok - drawing Phase 3 test pattern");
        draw_phase3_test_pattern(display);
    }

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

    Serial.println("[kernel_boot] scheduler started - boot banner printed, entering loop()");
}

void loop() {
    static uint32_t last_tick_ms = 0;
    uint32_t now_ms = millis();
    uint32_t dt = now_ms - last_tick_ms;
    last_tick_ms = now_ms;
    if (dt == 0) {
        dt = 1;
    } else if (dt > 50) {
        dt = 50; /* clamp after any stall, same as sim_main.c */
    }

    time_set_now_us((time_us_t)now_ms * 1000);

    for (uint32_t t = 0; t < dt; t++) {
        scheduler_tick();
    }
    scheduler_step();
    timers_process();
}
