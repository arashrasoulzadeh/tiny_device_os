// ArdubotOS kernel boot wrapper for NodeMCU (ESP8266 Arduino).
//
// Arduino's setup()/loop() runs here. Apps are drawn via stdapps_install()
// and app_start(). The scheduler is kernel/scheduler.c (generic setjmp/longjmp
// fiber scheduler with host_stack.c Xtensa impl), not FreeRTOS.
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
#include "app.h"
#include "app_types.h"
#include "ardubot_keys.h"
#include "hal_display.h"
#include "hal_power.h"
#include "hal_storage.h"
#include "host_stack.h"
#include "littlefs_vfs.h"
#include "os_boot.h"
#include "os_clock.h"
#include "os_time.h"
#include "power.h"
#include "scheduler.h"
#include "sensor_service.h"
#include "sim_gpio.h"
#include "stdapps_register.h"
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

// Bridges the board's two real buttons (D5=GPIO14=UP, D6=GPIO12=SELECT)
// into sim_gpio's key-state table. Apps in apps/stdapps/ bind keys through
// that table. UP is held as SIM_KEY_UP. A short SELECT press is
// SIM_KEY_ENTER; holding SELECT is SIM_KEY_ESCAPE (back to the launcher).
#define KERNEL_BOOT_BTN_UP_GPIO 14     // D5
#define KERNEL_BOOT_BTN_SELECT_GPIO 12 // D6
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

static hal_power_t *g_board_power;

extern "C" {

static int board_mount_storage(void) {
  hal_storage_t *flash =
      hal_storage_open("/dev/flash0", HAL_STORAGE_TYPE_FLASH);
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

static int board_read_temp(int32_t *temp_c) {
  int32_t decideg = 0;
  if (!temp_c) {
    return -1;
  }
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
  static const uint32_t esp8266_freqs[] = {160, 80, 40, 20, 10};
  power_init();
  g_board_power = hal_power_open("/dev/power0");
  if (g_board_power) {
    (void)hal_power_init(g_board_power);
  }
  (void)power_set_available_freqs(esp8266_freqs, 5);
  power_governor_set_actuators(board_set_freq, board_set_brightness,
                               board_read_temp);
}

static task_tcb_t *g_demo_task_tcb;

static void demo_task(void *arg) {
  (void)arg;
  for (;;) {
    task_sleep(1000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(
      "[kernel_boot] ArdubotOS real kernel starting (NodeMCU ESP8266)");
  install_board_tz();

  pinMode(KERNEL_BOOT_BTN_UP_GPIO, INPUT_PULLUP);
  pinMode(KERNEL_BOOT_BTN_SELECT_GPIO, INPUT_PULLUP);
  sim_gpio_init();

  if (scheduler_init() != 0) {
    Serial.println("[kernel_boot] scheduler_init failed");
    return;
  }
  bind_power_governor();

  if (task_create("demo", demo_task, NULL, TASK_PRIO_NORMAL,
                  HOST_STACK_MIN_BYTES * 2, &g_demo_task_tcb) != 0) {
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
  const char *start = stdapps_start_name();
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

  if (dt == 0) {
    delay(1);
    return;
  }
  if (dt > 50) {
    dt = 50;
  }
  last_tick_ms = now_ms;

  time_set_now_us((time_us_t)now_ms * 1000);

  for (uint32_t t = 0; t < dt; t++) {
    scheduler_tick();
  }
  scheduler_step();
  timers_process();

  // Small yield to keep ESP8266 watchdog happy
  delay(1);
}
