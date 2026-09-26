#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_power hal_power_t;

hal_power_t* hal_power_open(const char* path);
void hal_power_close(hal_power_t* power);

int hal_power_init(hal_power_t* power);
int hal_power_set_cpu_freq(hal_power_t* power, uint32_t freq_mhz);
uint32_t hal_power_get_cpu_freq(const hal_power_t* power);

// CPU frequency scaling - get available frequencies
int hal_power_get_available_freqs(const hal_power_t* power, uint32_t* freqs, uint32_t* count);

int hal_power_light_sleep(hal_power_t* power, uint32_t timeout_ms);
int hal_power_deep_sleep(hal_power_t* power, uint32_t timeout_ms);

int hal_power_add_gpio_wake(hal_power_t* power, int gpio_num, hal_gpio_irq_t trigger);
int hal_power_remove_gpio_wake(hal_power_t* power, int gpio_num);
int hal_power_add_rtc_wake(hal_power_t* power, uint32_t timeout_ms);
int hal_power_add_uart_wake(hal_power_t* power, int uart_num);

typedef enum {
    ESP_SLEEP_WAKEUP_UNDEFINED = 0,
    ESP_SLEEP_WAKEUP_TIMER,
    ESP_SLEEP_WAKEUP_GPIO,
    ESP_SLEEP_WAKEUP_UART,
    ESP_SLEEP_WAKEUP_TOUCHPAD,
    ESP_SLEEP_WAKEUP_EXT0,
    ESP_SLEEP_WAKEUP_EXT1,
    ESP_SLEEP_WAKEUP_ULP,
    ESP_SLEEP_WAKEUP_BT,
    ESP_SLEEP_WAKEUP_WIFI,
    ESP_SLEEP_WAKEUP_ALL,
} esp_sleep_wakeup_cause_t;

esp_sleep_wakeup_cause_t hal_power_get_wake_cause(hal_power_t* power);
const char* hal_power_get_wake_cause_str(esp_sleep_wakeup_cause_t cause);

int hal_power_configure_ulp(hal_power_t* power, const void* ulp_program, size_t size);
int hal_power_start_ulp(hal_power_t* power);
int hal_power_stop_ulp(hal_power_t* power);

uint32_t hal_power_get_rtc_time_ms(hal_power_t* power);
int hal_power_set_rtc_time(hal_power_t* power, uint32_t epoch_ms);

const char* hal_power_get_path(const hal_power_t* power);

#ifdef __cplusplus
}
#endif