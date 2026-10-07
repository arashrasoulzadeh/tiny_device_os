#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif

// Use power_mode_t from scheduler.h
// typedef enum {
//     POWER_MODE_ACTIVE = 0,
//     POWER_MODE_IDLE,
//     POWER_MODE_LIGHT_SLEEP,
//     POWER_MODE_DEEP_SLEEP
// } power_mode_t;

// Deep sleep wake sources (match hal_power.h)
typedef enum {
    POWER_WAKE_NONE = 0,
    POWER_WAKE_TIMER = 1,
    POWER_WAKE_GPIO = 2,
    POWER_WAKE_UART = 3,
    POWER_WAKE_TOUCH = 4,
    POWER_WAKE_ULP = 5,
} power_wake_source_t;

// Wake configuration
typedef struct {
    uint32_t timeout_ms;          // 0 = infinite
    int gpio_num;                 // -1 = disabled
    int gpio_trigger;             // HAL_GPIO_IRQ_RISING/FALLING/BOTH
} power_sleep_config_t;

// Wake result
typedef struct {
    bool woke_up;
    int wake_gpio;                // -1 = timer, >=0 = GPIO pin
    uint32_t slept_ms;
} power_wake_result_t;

typedef power_wake_result_t wake_result_t;

// Power management API
int power_init(void);
void power_deinit(void);

// Wake result
wake_result_t get_wake_result(void);
const char* wake_source_str(int wake_source);

// Sleep modes
int power_light_sleep(uint32_t timeout_ms);
int power_deep_sleep(uint32_t timeout_ms);
int power_deep_sleep_with_gpio_wake(int gpio_num, int trigger, uint32_t timeout_ms);
int power_deep_sleep_with_gpio_wake_ext(int gpio_num, int trigger, uint32_t timeout_ms);

// CPU frequency scaling (ESP32-specific)
typedef struct {
    uint32_t freq_mhz;
    const char* name;
} cpu_freq_option_t;

int power_set_cpu_freq(uint32_t freq_mhz);
uint32_t power_get_cpu_freq(void);
int power_get_available_freqs(uint32_t* freqs, uint32_t max_count);

// Wake source configuration
int power_add_gpio_wake(int gpio_num, int trigger);  // HAL_GPIO_IRQ_RISING/FALLING/BOTH
int power_remove_gpio_wake(int gpio_num);
int power_add_rtc_wake(uint32_t timeout_ms);
int power_remove_rtc_wake(void);

// RTC time
uint32_t get_rtc_time_ms(void);
int set_rtc_time(uint32_t epoch_ms);

// Battery/Power monitoring (stub for now)
typedef struct {
    float voltage_v;
    float current_ma;
    float battery_percent;
    bool charging;
} power_battery_info_t;

int power_get_battery_info(power_battery_info_t* info);
int power_set_battery_monitor_interval(uint32_t interval_ms);

// Per-driver PM callbacks (called by scheduler)
typedef void (*power_suspend_cb_t)(void* arg);
typedef void (*power_resume_cb_t)(void* arg);

int power_register_driver(void* driver_handle, 
                          void (*suspend)(void* arg), 
                          void (*resume)(void* arg),
                          void* arg);
int power_unregister_driver(void* driver_handle);

// Replace the frequency table the governor ranks. count is 1..8.
int power_set_available_freqs(const uint32_t* freqs, int count);

// Power state
power_mode_t get_power_mode(void);
bool is_in_sleep_mode(void);

/* How much compute a task wants. UNSET (zero) means "no hint": the governor
 * treats it as NORMAL. Apps never publish UNSET. The governor is the only
 * writer of the CPU clock; a hint is a ceiling, not a megahertz. */
typedef enum {
    POWER_DEMAND_UNSET = 0,
    POWER_DEMAND_IDLE = 1,
    POWER_DEMAND_LOW = 2,
    POWER_DEMAND_NORMAL = 3,
    POWER_DEMAND_HIGH = 4,
} power_demand_t;

typedef enum {
    POWER_LEVEL_COOL = 0,
    POWER_LEVEL_ECONOMY = 1,
    POWER_LEVEL_BALANCED = 2,
    POWER_LEVEL_PERFORMANCE = 3,
} power_level_t;

typedef int (*power_set_freq_fn)(uint32_t freq_mhz);
typedef int (*power_set_brightness_fn)(uint8_t cap);
typedef int (*power_read_temp_fn)(int32_t* temp_c);

int power_set_demand(task_tcb_t* task, power_demand_t demand);
power_demand_t power_get_demand(const task_tcb_t* task);

void power_governor_reset(void);
void power_governor_set_foreground_demand(power_demand_t demand);
power_demand_t power_governor_get_foreground_demand(void);

/* NULL set_freq keeps power_set_cpu_freq. NULL brightness is a no-op.
 * NULL read_temp leaves the thermal rule off. */
void power_governor_set_actuators(power_set_freq_fn set_freq,
                                  power_set_brightness_fn set_brightness,
                                  power_read_temp_fn read_temp);

void power_governor_note_idle(void);
void power_governor_note_busy(task_tcb_t* task);
uint32_t power_governor_idle_steps(void);
uint32_t power_governor_busy_steps(void);

/* Close a window every 100 ms of `now_ms`. next_wake_ms is the gap until
 * the next scheduled wake (UINT32_MAX = none). Does not call
 * power_light_sleep(); read the recommendation with
 * power_governor_light_sleep_ms(). */
void power_governor_tick(uint32_t now_ms, uint32_t next_wake_ms, bool any_runnable);

power_level_t power_governor_get_level(void);
uint32_t power_governor_light_sleep_ms(void);

#ifdef __cplusplus
}
#endif