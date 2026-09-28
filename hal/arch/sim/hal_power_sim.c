#include "hal_power.h"
#include <power.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct hal_power {
    char path[64];
};

hal_power_t* hal_power_open(const char* path) {
    hal_power_t* p = calloc(1, sizeof(hal_power_t));
    if (!p) return NULL;
    if (path) strncpy(p->path, path, sizeof(p->path) - 1);
    return p;
}

void hal_power_close(hal_power_t* power) {
    if (power) free(power);
}

int hal_power_init(hal_power_t* power) {
    (void)power;
    return power_init();
}

int hal_power_set_cpu_freq(hal_power_t* power, uint32_t freq_mhz) {
    (void)power;
    return power_set_cpu_freq(freq_mhz);
}

uint32_t hal_power_get_cpu_freq(const hal_power_t* power) {
    (void)power;
    return power_get_cpu_freq();
}

int hal_power_get_available_freqs(const hal_power_t* power, uint32_t* freqs, uint32_t* count) {
    (void)power;
    int cnt = power_get_available_freqs(freqs, count ? *count : 8);
    if (count) *count = cnt;
    return cnt >= 0 ? 0 : -1;
}

int hal_power_light_sleep(hal_power_t* power, uint32_t timeout_ms) {
    (void)power;
    return power_light_sleep(timeout_ms);
}

int hal_power_deep_sleep(hal_power_t* power, uint32_t timeout_ms) {
    (void)power;
    return power_deep_sleep(timeout_ms);
}

int hal_power_add_gpio_wake(hal_power_t* power, int gpio_num, hal_gpio_irq_t trigger) {
    (void)power;
    return power_add_gpio_wake(gpio_num, trigger);
}

int hal_power_remove_gpio_wake(hal_power_t* power, int gpio_num) {
    (void)power;
    return power_remove_gpio_wake(gpio_num);
}

int hal_power_add_rtc_wake(hal_power_t* power, uint32_t timeout_ms) {
    (void)power;
    return power_add_rtc_wake(timeout_ms);
}

int hal_power_add_uart_wake(hal_power_t* power, int uart_num) {
    (void)power;
    (void)uart_num;
    return -1;
}

esp_sleep_wakeup_cause_t hal_power_get_wake_cause(hal_power_t* power) {
    (void)power;
    wake_result_t wr = get_wake_result();
    if (!wr.woke_up) return 0; // ESP_SLEEP_WAKEUP_UNDEFINED
    if (wr.wake_gpio >= 0) return 3; // ESP_SLEEP_WAKEUP_GPIO
    return 2; // ESP_SLEEP_WAKEUP_TIMER
}

const char* hal_power_get_wake_cause_str(esp_sleep_wakeup_cause_t cause) {
    switch (cause) {
        case 1: return "TIMER";
        case 2: return "GPIO";
        case 3: return "UART";
        default: return "UNKNOWN";
    }
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
    return get_rtc_time_ms();
}

int hal_power_set_rtc_time(hal_power_t* power, uint32_t epoch_ms) {
    (void)power;
    return set_rtc_time(epoch_ms);
}

const char* hal_power_get_path(const hal_power_t* power) {
    return power ? power->path : NULL;
}