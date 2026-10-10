// hal_power_* implementation for ESP8266 Arduino framework.
// Uses Arduino/ESP8266 APIs instead of ESP-IDF esp_pm.h, esp_sleep.h, etc.
#include "hal_power.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

struct hal_power {
    char path[32];
    uint32_t max_freq_mhz;
    uint32_t min_freq_mhz;
    bool light_sleep_enable;
    bool initialized;
    esp_sleep_wakeup_cause_t last_wake_cause;
};

extern "C" {

hal_power_t* hal_power_open(const char* path) {
    hal_power_t* power = (hal_power_t*)calloc(1, sizeof(hal_power_t));
    if (!power) return NULL;

    strncpy(power->path, path, sizeof(power->path) - 1);
    
    // Use config value if available, otherwise default to 160
#ifdef ARDUBOT_CPU_MAX_FREQ_MHZ
    power->max_freq_mhz = ARDUBOT_CPU_MAX_FREQ_MHZ;
#else
    power->max_freq_mhz = 160;
#endif
    power->min_freq_mhz = 20;
    power->light_sleep_enable = true;
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

    // ESP8266 Arduino: set CPU frequency via system API
    // The actual frequency setting is done in hal_power_set_cpu_freq
    power->initialized = true;
    return 0;
}

int hal_power_set_cpu_freq(hal_power_t* power, uint32_t freq_mhz) {
    if (!power) return -1;

    if (freq_mhz > 160) freq_mhz = 160;
    if (freq_mhz < 20) freq_mhz = 20;

    // ESP8266 Arduino uses system_update_cpu_freq
    // Include the ESP8266-specific header
    extern void system_update_cpu_freq(uint32_t freq_mhz);
    system_update_cpu_freq(freq_mhz);
    power->max_freq_mhz = freq_mhz;
    return 0;
}

uint32_t hal_power_get_cpu_freq(const hal_power_t* power) {
    return power ? power->max_freq_mhz : 160;
}

int hal_power_get_available_freqs(const hal_power_t* power, uint32_t* freqs, uint32_t* count) {
    if (!power || !freqs || !count) return -1;
    
    // ESP8266 available frequencies based on max freq
#ifdef ARDUBOT_CPU_MAX_FREQ_MHZ
    uint32_t max_freq = ARDUBOT_CPU_MAX_FREQ_MHZ;
#else
    uint32_t max_freq = 160;
#endif
    
    // ESP8266 standard frequencies
    static const uint32_t esp8266_freqs[] = {max_freq, 80, 40, 20};
    uint32_t n = sizeof(esp8266_freqs) / sizeof(esp8266_freqs[0]);
    if (*count < n) return -1;
    memcpy(freqs, esp8266_freqs, n * sizeof(uint32_t));
    *count = n;
    return 0;
}

int hal_power_get_die_temp_c(hal_power_t* power, int32_t* temp_c) {
    (void)power;
    if (!temp_c) return -1;
    // ESP8266 doesn't have a built-in temperature sensor accessible via Arduino
    return -1;
}

int hal_power_light_sleep(hal_power_t* power, uint32_t timeout_ms) {
    if (!power) return -1;

    // ESP8266 Arduino light sleep with timer wake
    // Use WiFi.forceSleepBegin/WiFi.forceSleepEnd for light sleep
    // But Arduino doesn't expose this easily. Return -1 for now.
    (void)timeout_ms;
    return -1;
}

int hal_power_deep_sleep(hal_power_t* power, uint32_t timeout_ms) {
    if (!power) return -1;

    // ESP8266 Arduino deep sleep
    // ESP.deepSleep(us) - but this restarts the chip
    if (timeout_ms > 0) {
        ESP.deepSleep(timeout_ms * 1000);
    } else {
        ESP.deepSleep(0);
    }
    return 0;  // Never returns
}

int hal_power_add_gpio_wake(hal_power_t* power, int gpio_num, hal_gpio_irq_t trigger) {
    (void)power;
    (void)gpio_num;
    (void)trigger;
    // ESP8266 Arduino doesn't support GPIO wake from deep sleep easily
    return -1;
}

int hal_power_remove_gpio_wake(hal_power_t* power, int gpio_num) {
    (void)power;
    (void)gpio_num;
    return -1;
}

int hal_power_add_rtc_wake(hal_power_t* power, uint32_t timeout_ms) {
    (void)power;
    (void)timeout_ms;
    // Handled by deep_sleep
    return 0;
}

int hal_power_add_uart_wake(hal_power_t* power, int uart_num) {
    (void)power;
    (void)uart_num;
    return -1;
}

esp_sleep_wakeup_cause_t hal_power_get_wake_cause(hal_power_t* power) {
    if (!power) return ESP_SLEEP_WAKEUP_UNDEFINED;
    // ESP8266 Arduino: ESP.getResetReason() or similar
    // For now return last known cause
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
    return millis();
}

int hal_power_set_rtc_time(hal_power_t* power, uint32_t epoch_ms) {
    (void)power; (void)epoch_ms;
    return 0;
}

const char* hal_power_get_path(const hal_power_t* power) {
    return power ? power->path : NULL;
}

} // extern "C"