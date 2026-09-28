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

// Power state
power_mode_t get_power_mode(void);
bool is_in_sleep_mode(void);

#ifdef __cplusplus
}
#endif