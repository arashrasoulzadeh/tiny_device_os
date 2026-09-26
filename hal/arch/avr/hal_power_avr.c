#include "hal_power.h"
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

typedef struct hal_power {
    char path[32];
    bool initialized;
} hal_power_t;

hal_power_t* hal_power_open(const char* path) {
    hal_power_t* power = calloc(1, sizeof(hal_power_t));
    if (!power) return NULL;
    
    strncpy(power->path, path, sizeof(power->path) - 1);
    power->initialized = false;
    
    return power;
}

void hal_power_close(hal_power_t* power) {
    if (!power) return;
    free(power);
}

int hal_power_init(hal_power_t* power) {
    if (!power || power->initialized) return -1;
    power->initialized = true;
    return 0;
}

int hal_power_set_cpu_freq(hal_power_t* power, uint32_t freq_mhz) {
    if (!power) return -1;
    (void)freq_mhz;
    return -1;
}

uint32_t hal_power_get_cpu_freq(const hal_power_t* power) {
    return power ? F_CPU / 1000000 : 16;
}

int hal_power_light_sleep(hal_power_t* power, uint32_t timeout_ms) {
    if (!power) return -1;
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();
    sleep_cpu();
    sleep_disable();
    return 0;
}

int hal_power_deep_sleep(hal_power_t* power, uint32_t timeout_ms) {
    if (!power) return -1;
    if (timeout_ms > 0) {
        wdt_enable(WDTO_8S);
    }
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu();
    sleep_disable();
    return 0;
}

int hal_power_add_gpio_wake(hal_power_t* power, int gpio_num, hal_gpio_irq_t trigger) {
    (void)power; (void)gpio_num; (void)trigger;
    return -1;
}

int hal_power_remove_gpio_wake(hal_power_t* power, int gpio_num) {
    (void)power; (void)gpio_num;
    return -1;
}

int hal_power_add_rtc_wake(hal_power_t* power, uint32_t timeout_ms) {
    (void)power; (void)timeout_ms;
    return -1;
}

int hal_power_add_uart_wake(hal_power_t* power, int uart_num) {
    (void)power; (void)uart_num;
    return -1;
}

esp_sleep_wakeup_cause_t hal_power_get_wake_cause(hal_power_t* power) {
    (void)power;
    return ESP_SLEEP_WAKEUP_UNDEFINED;
}

const char* hal_power_get_wake_cause_str(esp_sleep_wakeup_cause_t cause) {
    return "Unknown";
}

int hal_power_configure_ulp(hal_power_t* power, const void* ulp_program, size_t size) {
    (void)power; (void)ulp_program; (void)size;
    return -1;
}

int hal_power_start_ulp(hal_power_t* power) {
    (void)power;
    return -1;
}

int hal_power_stop_ulp(hal_power_t* power) {
    (void)power;
    return -1;
}

uint32_t hal_power_get_rtc_time_ms(hal_power_t* power) {
    (void)power;
    return 0;
}

int hal_power_set_rtc_time(hal_power_t* power, uint32_t epoch_ms) {
    (void)power; (void)epoch_ms;
    return 0;
}

const char* hal_power_get_path(const hal_power_t* power) {
    return power ? power->path : NULL;
}