#pragma once

#include "sensor_service.h"

#include <stddef.h>
#include <stdint.h>

/* One screen line for a sensor sample. Temperature is decidegrees Celsius. */
int sensors_format_line(char* buf, size_t cap, const char* key, sensor_type_t type, int32_t raw);
