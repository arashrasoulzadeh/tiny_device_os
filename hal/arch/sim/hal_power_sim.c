#include "hal_power.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char path[64];
    bool initialized;
    uint32_t cpu_freq_mhz;
    int wakeup_gpio[8];
    int wakeup_gpio_count;
    bool rtc_wake_enabled;
    uint32_t rtc_wake_time_ms;
    bool uart_wake_enabled[4];
} hal_power_sim_t;

static hal_power_sim_t g_power_sim = {0};

hal_power_t* hal_power_open(const char* path) {
    if (!path) return NULL;
    
    hal_power_sim_t* sim = &g_power_sim;
    if (sim->initialized) return NULL;
    
    strncpy(sim->path, path, sizeof(sim->path) - 1);
    sim->path[sizeof(sim->path) - 1] = '\0';
    sim->initialized = true;
    sim->cpu_freq_mhz = 240; // Default ESP32 max freq
    sim->wakeup_gpio_count = 0;
    sim->rtc_wake_enabled = false;
    memset(sim->uart_wake_enabled, 0, sizeof(sim->uart_wake_enabled));
    
    return (hal_power_t*)sim;
}

void hal_power_close(hal_power_t* power) {
    if (!power) return;
    hal_power_sim_t* sim = (hal_power_sim_t*)power;
    sim->initialized = false;
}

int hal_power_init(hal_power_t* power) {
    (void)power;
    return 0;
}

int hal_power_set_cpu_freq(hal_power_t* power, uint32_t freq_mhz) {
    if (!power) return -1;
    hal_power_sim_t* sim = (hal_power_sim_t*)power;
    sim->cpu_freq_mhz = freq_mhz;
    return 0;
}

uint32_t hal_power_get_cpu_freq(const hal_power_t* power) {
    if (!power) return 0;
    hal_power_sim_t* sim = (hal_power_sim_t*)power;
    return sim->cpu_freq_mhz;
}

int hal_power_get_available_freqs(const hal_power_t* power, uint32_t* freqs, uint32_t* count) {
    if (!power || !freqs || !count) return -1;
    
    // Simulate ESP32 available frequencies: 240, 160, 80, 40, 20, 10 MHz
    static const uint32_t available_freqs[] = {240, 160, 80, 40, 20, 10};
    uint32_t num_freqs = sizeof(available_freqs) / sizeof(available_freqs[0]);
    
    if (*count < num_freqs) {
        return -1; // Buffer too small
    }
    
    memcpy(freqs, available_freqs, num_freqs * sizeof(uint32_t));
    *count = num_freqs;
    return 0;
}

int hal_power_light_sleep(hal_power_t* power, uint32_t timeout_ms) {
    (void)power;
    (void)timeout_ms;
    return 0;
}

int hal_power_deep_sleep(hal_power_t* power, uint32_t timeout_ms) {
    (void)power;
    (void)timeout_ms;
    return 0;
}

int hal_power_add_gpio_wake(hal_power_t* power, int gpio_num, hal_gpio_irq_t trigger) {
    if (!power) return -1;
    hal_power_sim_t* sim = (hal_power_sim_t*)power;
    if (sim->wakeup_gpio_count >= 8) return -1;
    
    sim->wakeup_gpio[sim->wakeup_gpio_count++] = gpio_num;
    (void)trigger;
    return 0;
}

int hal_power_remove_gpio_wake(hal_power_t* power, int gpio_num) {
    if (!power) return -1;
    hal_power_sim_t* sim = (hal_power_sim_t*)power;
    
    for (int i = 0; i < sim->wakeup_gpio_count; i++) {
        if (sim->wakeup_gpio[i] == gpio_num) {
            for (int j = i; j < sim->wakeup_gpio_count - 1; j++) {
                sim->wakeup_gpio[j] = sim->wakeup_gpio[j + 1];
            }
            sim->wakeup_gpio_count--;
            return 0;
        }
    }
    return -1;
}

int hal_power_add_rtc_wake(hal_power_t* power, uint32_t timeout_ms) {
    if (!power) return -1;
    hal_power_sim_t* sim = (hal_power_sim_t*)power;
    sim->rtc_wake_enabled = true;
    sim->rtc_wake_time_ms = timeout_ms;
    return 0;
}

int hal_power_add_uart_wake(hal_power_t* power, int uart_num) {
    if (!power) return -1;
    hal_power_sim_t* sim = (hal_power_sim_t*)power;
    if (uart_num >= 0 && uart_num < 4) {
        sim->uart_wake_enabled[uart_num] = true;
        return 0;
    }
    return -1;
}

esp_sleep_wakeup_cause_t hal_power_get_wake_cause(hal_power_t* power) {
    (void)power;
    return ESP_SLEEP_WAKEUP_TIMER;
}

const char* hal_power_get_wake_cause_str(esp_sleep_wakeup_cause_t cause) {
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER: return "Timer";
        case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
        case ESP_SLEEP_WAKEUP_UART: return "UART";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "Touchpad";
        case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
        case ESP_SLEEP_WAKEUP_ULP: return "ULP";
        default: return "Unknown";
    }
}

int hal_power_configure_ulp(hal_power_t* power, const void* ulp_program, size_t size) {
    (void)power;
    (void)ulp_program;
    (void)size;
    return 0;
}

int hal_power_start_ulp(hal_power_t* power) {
    (void)power;
    return 0;
}

int hal_power_stop_ulp(hal_power_t* power) {
    (void)power;
    return 0;
}

uint32_t hal_power_get_rtc_time_ms(hal_power_t* power) {
    (void)power;
    return 0;
}

int hal_power_set_rtc_time(hal_power_t* power, uint32_t epoch_ms) {
    (void)power;
    (void)epoch_ms;
    return 0;
}

const char* hal_power_get_path(const hal_power_t* power) {
    if (!power) return NULL;
    hal_power_sim_t* sim = (hal_power_sim_t*)power;
    return sim->path;
}