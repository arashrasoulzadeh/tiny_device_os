#include "power.h"
#include "scheduler.h"
#include "os_time.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static struct {
    bool initialized;
    power_mode_t current_mode;
    wake_result_t last_wake;
    uint32_t rtc_time_ms;
    uint32_t cpu_freq_mhz;
    
    // Wake config
    struct {
        uint32_t timeout_ms;
        int gpio_num;
        int gpio_trigger;
    } wake_config;
    
    // CPU freq scaling
    uint32_t available_freqs[8];
    int num_available_freqs;
    uint32_t current_cpu_freq_mhz;
    
    // Driver PM callbacks
    struct {
        void* handle;
        void (*suspend)(void* arg);
        void (*resume)(void* arg);
        void* arg;
        bool registered;
    } drivers[8];
    int num_drivers;
} g_power = {0};

// Internal: get available CPU frequencies for ESP32
static void init_available_freqs(void) {
    g_power.num_available_freqs = 5;
    g_power.available_freqs[0] = 240;
    g_power.available_freqs[1] = 160;
    g_power.available_freqs[2] = 80;
    g_power.available_freqs[3] = 40;
    g_power.available_freqs[4] = 10;
    g_power.current_cpu_freq_mhz = 240;
}

// Get wake cause string
const char* wake_source_str(int wake_source) {
    switch (wake_source) {
        case 1: return "TIMER";
        case 2: return "GPIO";
        case 3: return "UART";
        default: return "UNKNOWN";
    }
}

// Get wake cause string (ESP compatibility)
const char* wake_source_str_esp(int wake_source) {
    return wake_source_str(wake_source);
}

// Initialize power management
int power_init(void) {
    if (g_power.initialized) {
        return 0;
    }
    
    init_available_freqs();
    g_power.initialized = true;
    g_power.current_mode = POWER_MODE_ACTIVE;
    g_power.rtc_time_ms = 0;
    g_power.current_cpu_freq_mhz = 240;
    g_power.wake_config.timeout_ms = 0;
    g_power.wake_config.gpio_num = -1;
    g_power.wake_config.gpio_trigger = 0;
    g_power.last_wake.woke_up = false;
    g_power.last_wake.wake_gpio = -1;
    g_power.last_wake.slept_ms = 0;
    
    // Initialize driver array
    for (int i = 0; i < 8; i++) {
        g_power.drivers[i].registered = false;
    }
    
    return 0;
}

void power_deinit(void) {
    g_power.initialized = false;
    g_power.current_mode = POWER_MODE_ACTIVE;
}

// Light sleep - CPU pauses but RAM retained
int power_light_sleep(uint32_t timeout_ms) {
    if (!g_power.initialized) return -1;
    
    uint32_t start = time_now_ms();
    g_power.current_mode = POWER_MODE_LIGHT_SLEEP;
    
    if (timeout_ms == 0) {
        // Infinite light sleep - yield until woken
        while (g_power.current_mode == POWER_MODE_LIGHT_SLEEP) {
            // On real hardware: WFI instruction
            // On simulator: yield
            time_sleep_ms(1);
        }
    } else {
        uint32_t end = start + timeout_ms;
        while (time_now_ms() < end && g_power.current_mode == POWER_MODE_LIGHT_SLEEP) {
            time_sleep_ms(1);
        }
    }
    
    g_power.current_mode = POWER_MODE_ACTIVE;
    return 0;
}

// Deep sleep - full power down with RTC wake
int power_deep_sleep(uint32_t timeout_ms) {
    return power_deep_sleep_with_gpio_wake(-1, 0, timeout_ms);
}

int power_deep_sleep_with_gpio_wake(int gpio_num, int trigger, uint32_t timeout_ms) {
    if (!g_power.initialized) return -1;
    
    g_power.wake_config.timeout_ms = timeout_ms;
    g_power.wake_config.gpio_num = gpio_num;
    g_power.wake_config.gpio_trigger = trigger;
    
    // Call driver suspend callbacks
    for (int i = 0; i < 8; i++) {
        if (g_power.drivers[i].registered && g_power.drivers[i].suspend) {
            g_power.drivers[i].suspend(g_power.drivers[i].arg);
        }
    }
    
    // Record sleep start
    uint32_t start = time_now_ms();
    
    if (timeout_ms == 0 && gpio_num == -1) {
        // Infinite deep sleep - in sim this just exits
        return 0;
    }
    
    // Simulate deep sleep by advancing time
    if (timeout_ms > 0) {
        time_sleep_ms(timeout_ms);
    }
    
    // Call driver resume callbacks
    for (int i = 0; i < 8; i++) {
        if (g_power.drivers[i].registered && g_power.drivers[i].resume) {
            g_power.drivers[i].resume(g_power.drivers[i].arg);
        }
    }
    
    // Record wake result
    g_power.last_wake.woke_up = true;
    g_power.last_wake.wake_gpio = -1;
    g_power.last_wake.slept_ms = time_now_ms() - start;
    
    return 0;
}

// CPU frequency scaling
int power_set_cpu_freq(uint32_t freq_mhz) {
    // Check if frequency is available
    for (int i = 0; i < g_power.num_available_freqs; i++) {
        if (g_power.available_freqs[i] == freq_mhz) {
            g_power.current_cpu_freq_mhz = freq_mhz;
            return 0;
        }
    }
    return -1; // Invalid frequency
}

uint32_t power_get_cpu_freq(void) {
    return g_power.current_cpu_freq_mhz;
}

int power_get_available_freqs(uint32_t* freqs, uint32_t max_count) {
    if (!freqs || max_count == 0) return -1;
    
    uint32_t count = (uint32_t)g_power.num_available_freqs < max_count ? (uint32_t)g_power.num_available_freqs : max_count;
    for (uint32_t i = 0; i < count; i++) {
        freqs[i] = g_power.available_freqs[i];
    }
    return count;
}

// Wake source configuration
int power_add_gpio_wake(int gpio_num, int trigger) {
    g_power.wake_config.gpio_num = gpio_num;
    g_power.wake_config.gpio_trigger = trigger;
    return 0;
}

int power_remove_gpio_wake(int gpio_num) {
    (void)gpio_num;
    g_power.wake_config.gpio_num = -1;
    return 0;
}

int power_add_rtc_wake(uint32_t timeout_ms) {
    g_power.wake_config.timeout_ms = timeout_ms;
    return 0;
}

int power_remove_rtc_wake(void) {
    g_power.wake_config.timeout_ms = 0;
    return 0;
}

// RTC time
uint32_t get_rtc_time_ms(void) {
    return g_power.rtc_time_ms;
}

int set_rtc_time(uint32_t epoch_ms) {
    g_power.rtc_time_ms = epoch_ms;
    return 0;
}

// Battery info
int power_get_battery_info(power_battery_info_t* info) {
    if (!info) return -1;
    info->voltage_v = 3.7f;
    info->current_ma = 0.0f;
    info->battery_percent = 100.0f;
    info->charging = false;
    return 0;
}

int power_set_battery_monitor_interval(uint32_t interval_ms) {
    (void)interval_ms;
    return 0;
}

// Driver PM callbacks
int power_register_driver(void* driver_handle, 
                          void (*suspend)(void* arg), 
                          void (*resume)(void* arg),
                          void* arg) {
    for (int i = 0; i < 8; i++) {
        if (!g_power.drivers[i].registered) {
            g_power.drivers[i].handle = driver_handle;
            g_power.drivers[i].suspend = suspend;
            g_power.drivers[i].resume = resume;
            g_power.drivers[i].arg = arg;
            g_power.drivers[i].registered = true;
            return 0;
        }
    }
    return -1; // No space
}

int power_unregister_driver(void* driver_handle) {
    for (int i = 0; i < 8; i++) {
        if (g_power.drivers[i].registered && g_power.drivers[i].handle == driver_handle) {
            g_power.drivers[i].registered = false;
            return 0;
        }
    }
    return -1;
}

// Power state
power_mode_t get_power_mode(void) {
    return g_power.current_mode;
}

bool is_in_sleep_mode(void) {
    return g_power.current_mode != POWER_MODE_ACTIVE;
}

#ifdef __cplusplus
}
#endif