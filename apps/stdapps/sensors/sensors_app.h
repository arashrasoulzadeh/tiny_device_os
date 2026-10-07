#pragma once

#include "sensor_service.h"

#include <stddef.h>
#include <stdint.h>

/* One screen line for a sensor sample.
 * Temperature is decidegrees Celsius. CPU and RAM are percents.
 * Power is the CPU clock in MHz, or "--" when the target has no clock reading.
 * This board has no current shunt, so the power line is not milliamps. */
int sensors_format_line(char* buf, size_t cap, const char* key, sensor_type_t type, int32_t raw);
