#include "sensor_service.h"
#include "os_time.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ARDUBOT_TARGET_ESP32)
#include "driver/temperature_sensor.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include "alloc.h"
#include "hal_adc.h"
#include "scheduler.h"
#endif

#if __has_include("device_config.h")
#include "device_config.h"
#endif

#define SENSOR_KEY_MAX 16
#define SENSOR_PATH_MAX 32

typedef struct {
    char key[SENSOR_KEY_MAX];
    sensor_type_t type;
    char path[SENSOR_PATH_MAX];
    uint32_t refresh_ms;
    void* hw;
    int32_t value;
    uint32_t sampled_ms;
    bool has_sample;
    bool used;
} sensor_slot_t;

static sensor_slot_t g_slots[SENSOR_SERVICE_MAX];
static uint32_t g_samples;

#if defined(ARDUBOT_TARGET_ESP32)
static temperature_sensor_handle_t g_temp;
static int g_temp_ready;
#endif

static int sample_temp(int32_t* value) {
#if defined(ARDUBOT_TARGET_ESP32)
    float celsius = 0.0f;
    if (!g_temp_ready) {
        temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
        if (temperature_sensor_install(&cfg, &g_temp) != ESP_OK) {
            return -1;
        }
        if (temperature_sensor_enable(g_temp) != ESP_OK) {
            return -1;
        }
        g_temp_ready = 1;
    }
    if (temperature_sensor_get_celsius(g_temp, &celsius) != ESP_OK) {
        return -1;
    }
    *value = (int32_t)(celsius * 10.0f);
#else
    *value = 250;
#endif
    g_samples++;
    return 0;
}

#if defined(ARDUBOT_TARGET_ESP32)
static int g_cpu_ready;
static uint32_t g_idle_prev;
static uint32_t g_total_prev;
#endif

int32_t sensor_cpu_busy_percent(uint64_t idle, uint64_t total) {
    if (total == 0 || idle >= total) {
        return 0;
    }
    return (int32_t)(((total - idle) * 100ull) / total);
}

static int sample_cpu(int32_t* value) {
#if defined(ARDUBOT_TARGET_ESP32)
    /* FreeRTOS already accounts idle-task time, including time spent waiting
     * for an interrupt. A self-calibrated idle-hook rate flips between 0 and
     * 100 because the hook runs once per interrupt and the loop task used to
     * busy-spin. */
    uint32_t idle = (uint32_t)ulTaskGetIdleRunTimeCounter();
    uint32_t total = (uint32_t)portGET_RUN_TIME_COUNTER_VALUE();
    uint32_t idle_d;
    uint32_t total_d;
    if (!g_cpu_ready) {
        g_cpu_ready = 1;
        g_idle_prev = idle;
        g_total_prev = total;
        *value = 0;
        g_samples++;
        return 0;
    }
    idle_d = idle - g_idle_prev;
    total_d = total - g_total_prev;
    g_idle_prev = idle;
    g_total_prev = total;
    *value = sensor_cpu_busy_percent(idle_d, total_d);
#else
    *value = sensor_cpu_busy_percent(scheduler_get_idle_tick_count(), scheduler_get_tick_count());
#endif
    g_samples++;
    return 0;
}

static int sample_ram(int32_t* value) {
#if defined(ARDUBOT_TARGET_ESP32)
    size_t total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t freeb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (total == 0) {
        total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
        freeb = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    }
#else
    size_t total = os_get_heap_total();
    size_t freeb = os_get_free_heap();
#endif
    if (total == 0 || freeb >= total) {
        *value = 0;
    } else {
        *value = (int32_t)(((total - freeb) * 100u) / total);
    }
    g_samples++;
    return 0;
}

static int sample_power(int32_t* value) {
#if defined(ARDUBOT_TARGET_ESP32)
    uint32_t mhz = esp_rom_get_cpu_ticks_per_us();
    *value = mhz > 0 ? (int32_t)mhz : -1;
#else
    *value = -1;
#endif
    g_samples++;
    return 0;
}

static int sample_meter(sensor_slot_t* slot, int32_t* value) {
    if (slot->type == SENSOR_TYPE_TEMP) {
        return sample_temp(value);
    }
    if (slot->type == SENSOR_TYPE_CPU) {
        return sample_cpu(value);
    }
    if (slot->type == SENSOR_TYPE_RAM) {
        return sample_ram(value);
    }
    if (slot->type == SENSOR_TYPE_POWER) {
        return sample_power(value);
    }
    return 1;
}

#if defined(ARDUBOT_TARGET_ESP32)
static adc_oneshot_unit_handle_t g_adc;
static int g_adc_ready;

static int channel_from_path(const char* path) {
    const char* digits = path;
    int channel;
    if (!path) {
        return 0;
    }
    if (strncmp(path, "/dev/adc", 8) == 0) {
        digits = path + 8;
    } else if (strncmp(path, "adc", 3) == 0) {
        digits = path + 3;
    }
    channel = (int)strtol(digits, NULL, 10);
    if (channel < 0 || channel > 7) {
        return 0;
    }
    return channel;
}

static int sample_hw(sensor_slot_t* slot, int32_t* value) {
    adc_oneshot_chan_cfg_t chan = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    int raw = 0;
    int channel;
    int meter = sample_meter(slot, value);
    if (meter <= 0) {
        return meter;
    }
    channel = channel_from_path(slot->path);
    if (!g_adc_ready) {
        adc_oneshot_unit_init_cfg_t cfg = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        if (adc_oneshot_new_unit(&cfg, &g_adc) != ESP_OK) {
            return -1;
        }
        g_adc_ready = 1;
    }
    if (adc_oneshot_config_channel(g_adc, (adc_channel_t)channel, &chan) != ESP_OK) {
        return -1;
    }
    if (adc_oneshot_read(g_adc, (adc_channel_t)channel, &raw) != ESP_OK) {
        return -1;
    }
    *value = raw;
    g_samples++;
    return 0;
}
#else
static int sample_hw(sensor_slot_t* slot, int32_t* value) {
    uint16_t raw = 0;
    hal_adc_t* adc;
    int meter = sample_meter(slot, value);
    if (meter <= 0) {
        return meter;
    }
    adc = (hal_adc_t*)slot->hw;
    if (!adc) {
        adc = hal_adc_open(slot->path[0] ? slot->path : "/dev/adc0");
        if (!adc || hal_adc_init(adc) != 0) {
            if (adc) {
                hal_adc_close(adc);
            }
            return -1;
        }
        slot->hw = adc;
    }
    if (hal_adc_read(adc, &raw) != 0) {
        return -1;
    }
    *value = (int32_t)raw;
    g_samples++;
    return 0;
}
#endif

static sensor_slot_t* find_slot(const char* key) {
    int i;
    if (!key || !key[0]) {
        return NULL;
    }
    for (i = 0; i < SENSOR_SERVICE_MAX; i++) {
        if (g_slots[i].used && strcmp(g_slots[i].key, key) == 0) {
            return &g_slots[i];
        }
    }
    return NULL;
}

int sensor_service_reset(void) {
    int i;
    for (i = 0; i < SENSOR_SERVICE_MAX; i++) {
#if !defined(ARDUBOT_TARGET_ESP32)
        if (g_slots[i].hw) {
            hal_adc_close((hal_adc_t*)g_slots[i].hw);
        }
#endif
        memset(&g_slots[i], 0, sizeof(g_slots[i]));
    }
    g_samples = 0;
    return 0;
}

int sensor_service_add(const char* key, sensor_type_t type, const char* path, uint32_t refresh_ms) {
    int i;
    if (!key || !key[0] || strlen(key) >= SENSOR_KEY_MAX) {
        return -1;
    }
    if (type != SENSOR_TYPE_ADC && type != SENSOR_TYPE_TEMP && type != SENSOR_TYPE_CPU &&
        type != SENSOR_TYPE_RAM && type != SENSOR_TYPE_POWER) {
        return -1;
    }
    if (find_slot(key)) {
        return -1;
    }
    for (i = 0; i < SENSOR_SERVICE_MAX; i++) {
        if (!g_slots[i].used) {
            memset(&g_slots[i], 0, sizeof(g_slots[i]));
            strncpy(g_slots[i].key, key, SENSOR_KEY_MAX - 1);
            g_slots[i].type = type;
            if (type == SENSOR_TYPE_ADC && (!path || !path[0])) {
                path = "/dev/adc0";
            } else if (!path) {
                path = "";
            }
            strncpy(g_slots[i].path, path, SENSOR_PATH_MAX - 1);
            g_slots[i].refresh_ms = refresh_ms;
            g_slots[i].used = true;
            return 0;
        }
    }
    return -1;
}

int sensor_service_load_builtin(void) {
    int n = 0;
#ifdef ARDUBOT_SENSOR_0_KEY
    if (sensor_service_add(ARDUBOT_SENSOR_0_KEY, ARDUBOT_SENSOR_0_TYPE, ARDUBOT_SENSOR_0_PATH,
                           ARDUBOT_SENSOR_0_REFRESH_MS) == 0) {
        n++;
    }
#endif
#ifdef ARDUBOT_SENSOR_1_KEY
    if (sensor_service_add(ARDUBOT_SENSOR_1_KEY, ARDUBOT_SENSOR_1_TYPE, ARDUBOT_SENSOR_1_PATH,
                           ARDUBOT_SENSOR_1_REFRESH_MS) == 0) {
        n++;
    }
#endif
#ifdef ARDUBOT_SENSOR_2_KEY
    if (sensor_service_add(ARDUBOT_SENSOR_2_KEY, ARDUBOT_SENSOR_2_TYPE, ARDUBOT_SENSOR_2_PATH,
                           ARDUBOT_SENSOR_2_REFRESH_MS) == 0) {
        n++;
    }
#endif
#ifdef ARDUBOT_SENSOR_3_KEY
    if (sensor_service_add(ARDUBOT_SENSOR_3_KEY, ARDUBOT_SENSOR_3_TYPE, ARDUBOT_SENSOR_3_PATH,
                           ARDUBOT_SENSOR_3_REFRESH_MS) == 0) {
        n++;
    }
#endif
#ifdef ARDUBOT_SENSOR_4_KEY
    if (sensor_service_add(ARDUBOT_SENSOR_4_KEY, ARDUBOT_SENSOR_4_TYPE, ARDUBOT_SENSOR_4_PATH,
                           ARDUBOT_SENSOR_4_REFRESH_MS) == 0) {
        n++;
    }
#endif
#ifdef ARDUBOT_SENSOR_5_KEY
    if (sensor_service_add(ARDUBOT_SENSOR_5_KEY, ARDUBOT_SENSOR_5_TYPE, ARDUBOT_SENSOR_5_PATH,
                           ARDUBOT_SENSOR_5_REFRESH_MS) == 0) {
        n++;
    }
#endif
#ifdef ARDUBOT_SENSOR_6_KEY
    if (sensor_service_add(ARDUBOT_SENSOR_6_KEY, ARDUBOT_SENSOR_6_TYPE, ARDUBOT_SENSOR_6_PATH,
                           ARDUBOT_SENSOR_6_REFRESH_MS) == 0) {
        n++;
    }
#endif
#ifdef ARDUBOT_SENSOR_7_KEY
    if (sensor_service_add(ARDUBOT_SENSOR_7_KEY, ARDUBOT_SENSOR_7_TYPE, ARDUBOT_SENSOR_7_PATH,
                           ARDUBOT_SENSOR_7_REFRESH_MS) == 0) {
        n++;
    }
#endif
    if (sensor_service_add("cpu", SENSOR_TYPE_CPU, "", 1000) == 0) {
        n++;
    }
    if (sensor_service_add("ram", SENSOR_TYPE_RAM, "", 1000) == 0) {
        n++;
    }
    if (sensor_service_add("pwr", SENSOR_TYPE_POWER, "", 1000) == 0) {
        n++;
    }
    return n;
}

static sensor_slot_t* slot_at(int index) {
    int i;
    int n = 0;
    if (index < 0) {
        return NULL;
    }
    for (i = 0; i < SENSOR_SERVICE_MAX; i++) {
        if (!g_slots[i].used) {
            continue;
        }
        if (n == index) {
            return &g_slots[i];
        }
        n++;
    }
    return NULL;
}

int sensor_service_count(void) {
    int i;
    int n = 0;
    for (i = 0; i < SENSOR_SERVICE_MAX; i++) {
        if (g_slots[i].used) {
            n++;
        }
    }
    return n;
}

const char* sensor_service_key(int index) {
    sensor_slot_t* slot = slot_at(index);
    return slot ? slot->key : NULL;
}

sensor_type_t sensor_service_type(int index) {
    sensor_slot_t* slot = slot_at(index);
    return slot ? slot->type : (sensor_type_t)0;
}

uint32_t sensor_service_samples(void) {
    return g_samples;
}

int sensor_get_at(const char* key, uint32_t now_ms, int32_t* value) {
    sensor_slot_t* slot = find_slot(key);
    int32_t sample = 0;
    if (!slot || !value) {
        return -1;
    }
    if (slot->has_sample && slot->refresh_ms > 0 &&
        (uint32_t)(now_ms - slot->sampled_ms) < slot->refresh_ms) {
        *value = slot->value;
        return 0;
    }
    if (sample_hw(slot, &sample) != 0) {
        return -1;
    }
    slot->value = sample;
    slot->sampled_ms = now_ms;
    slot->has_sample = true;
    *value = sample;
    return 0;
}

int sensor_get(const char* key, int32_t* value) {
    return sensor_get_at(key, time_now_ms(), value);
}
