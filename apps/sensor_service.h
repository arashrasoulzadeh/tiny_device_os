#pragma once

/**
 * Sensor service.
 *
 * Sensors are declared in the device config under `sensors:` (key, type,
 * optional path, refresh_ms). Boot registers that list.
 * app_helper_sensor() / sensor_get() return the last sample for a key and
 * only touch the hardware again after refresh_ms.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SENSOR_TYPE_ADC = 1,
    SENSOR_TYPE_TEMP = 2,
} sensor_type_t;

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
