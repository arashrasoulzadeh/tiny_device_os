#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* os_name;
    const char* os_version;
    const char* target;
    const char* arch;
    uint16_t display_w;
    uint16_t display_h;
    uint32_t uptime_ms;
    uint32_t installed_apps;
} device_info_t;

/** Fill @p out with current device / OS snapshot. Returns 0 on success. */
int device_info_query(device_info_t* out);

#ifdef __cplusplus
}
#endif
