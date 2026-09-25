#include "hal_power.h"
#include <esp_pm.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <driver/rtc_cntl.h>
#include <soc/rtc.h>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "hal_power";

typedef struct hal_power {
    char path[32];
    esp_pm_config_t pm_config;
    bool initialized;
    esp_sleep_wakeup_cause_t last_wake_cause;
} hal_power_t;

hal_power_t* hal_power_open(const char* path) {
    hal_power_t* power = calloc(1, sizeof(hal_power_t));
    if (!power) return NULL;
    
    strncpy(power->path, path, sizeof(power->path) - 1);
    power->initialized = false;
    power->last_wake_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
    
    return power;
}

void hal_power_close(hal_power_t* power) {
    if (!power) return;
    free(power);
}

int hal_power_init(hal_power_t* power) {
    if (!power || power->initialized) return -1;
    
    power->pm_config = (esp_pm_config_t){
        .max_freq_mhz = 160,
        .min_freq_mhz = 20,
        .light_sleep_enable = true,
    };
    
    esp_err_t err = esp_pm_configure(&power->pm_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure power management: %s", esp_err_to_name(err));
        return -1;
    }
    
    power->initialized = true;
    return 0;
}

int hal_power_set_cpu_freq(hal_power_t* power, uint32_t freq_mhz) {
    if (!power) return -1;
    
    if (freq_mhz > 160) freq_mhz = 160;
    if (freq_mhz < 20) freq_mhz = 20;
    
    power->pm_config.max_freq_mhz = freq_mhz;
    esp_err_t err = esp_pm_configure(&power->pm_config);
    return err == ESP_OK ? 0 : -1;
}

uint32_t hal_power_get_cpu_freq(const hal_power_t* power) {
    return power ? power->pm_config.max_freq_mhz : 160;
}

int hal_power_light_sleep(hal_power_t* power, uint32_t timeout_ms) {
    if (!power) return -1;
    
    if (timeout_ms > 0) {
        esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
    }
    
    esp_err_t err = esp_light_sleep_start();
    return err == ESP_OK ? 0 : -1;
}

int hal_power_deep_sleep(hal_power_t* power, uint32_t timeout_ms) {
    if (!power) return -1;
    
    if (timeout_ms > 0) {
        esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
    }
    
    esp_deep_sleep_start();
    return 0;
}

int hal_power_add_gpio_wake(hal_power_t* power, int gpio_num, hal_gpio_irq_t trigger) {
    if (!power) return -1;
    
    gpio_pull_mode_t pull = GPIO_PULLUP_ONLY;
    if (trigger == HAL_GPIO_IRQ_FALLING) {
        pull = GPIO_PULLDOWN_ONLY;
    }
    
    gpio_pullup_en(gpio_num);
    gpio_pulldown_en(gpio_num);
    
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable(gpio_num, (trigger == HAL_GPIO_IRQ_RISING) ? GPIO_INTR_HIGH_LEVEL : 
                        (trigger == HAL_GPIO_IRQ_FALLING) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_ANYEDGE);
    
    return 0;
}

int hal_power_remove_gpio_wake(hal_power_t* power, int gpio_num) {
    if (!power) return -1;
    gpio_wakeup_disable(gpio_num);
    return 0;
}

int hal_power_add_rtc_wake(hal_power_t* power, uint32_t timeout_ms) {
    if (!power) return -1;
    esp_sleep_enable_timer_wakeup(timeout_ms * 1000);
    return 0;
}

int hal_power_add_uart_wake(hal_power_t* power, int uart_num) {
    if (!power) return -1;
    uart_set_wakeup_threshold(uart_num, 3);
    esp_sleep_enable_uart_wakeup(uart_num);
    return 0;
}

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

esp_sleep_wakeup_cause_t hal_power_get_wake_cause(hal_power_t* power) {
    if (!power) return ESP_SLEEP_WAKEUP_UNDEFINED;
    power->last_wake_cause = esp_sleep_get_wakeup_cause();
    return power->last_wake_cause;
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
        case ESP_SLEEP_WAKEUP_BT: return "Bluetooth";
        case ESP_SLEEP_WAKEUP_WIFI: return "WiFi";
        case ESP_SLEEP_WAKEUP_ALL: return "All";
        default: return "Unknown";
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
    return rtc_time_get() * 1000;
}

int hal_power_set_rtc_time(hal_power_t* power, uint32_t epoch_ms) {
    (void)power; (void)epoch_ms;
    return 0;
}

const char* hal_power_get_path(const hal_power_t* power) {
    return power ? power->path : NULL;
}