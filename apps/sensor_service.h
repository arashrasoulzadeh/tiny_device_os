#pragma once

/**
 * Sensor service.
 *
 * Sensors are declared in the device config under `sensors:` (key, type,
 * optional path, refresh_ms). Boot registers that list, then always adds
 * cpu (busy percent), ram (heap used percent), and pwr (CPU clock in MHz,
 * or -1 when the target has no clock). A board with no current shunt does
 * not report milliamps. app_helper_sensor() / sensor_get() return the last
 * sample for a key and only touch the hardware again after refresh_ms.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SENSOR_TYPE_ADC = 1,
    SENSOR_TYPE_TEMP = 2,
    SENSOR_TYPE_CPU = 3,   /* busy percent over the refresh window, 0-100 */
    SENSOR_TYPE_RAM = 4,   /* heap used percent, 0-100 */
    SENSOR_TYPE_POWER = 5, /* CPU clock in MHz; -1 when the target has none */
} sensor_type_t;

/* Busy share of a window. idle and total are the same unit (ticks or microseconds). */
int32_t sensor_cpu_busy_percent(uint64_t idle, uint64_t total);

#define SENSOR_SERVICE_MAX 8

int sensor_service_reset(void);
int sensor_service_add(const char* key, sensor_type_t type, const char* path, uint32_t refresh_ms);
int sensor_service_load_builtin(void);
int sensor_service_count(void);
const char* sensor_service_key(int index);
sensor_type_t sensor_service_type(int index);

/** Hardware samples taken. A cached read does not increase this. */
uint32_t sensor_service_samples(void);

/** Latest sample for `key`. 0 on success, -1 if the key is missing or the read fails. */
int sensor_get(const char* key, int32_t* value);
int sensor_get_at(const char* key, uint32_t now_ms, int32_t* value);

#ifdef __cplusplus
}
#endif
