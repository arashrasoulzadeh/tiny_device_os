#pragma once

/**
 * Splash sequence shared by the simulator and the board boot.
 * Caller installs apps first so the bar total includes every app.
 * Storage is a hook: the sim opens the config store, the board mounts LittleFS.
 */

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*os_boot_storage_fn)(void);

typedef struct {
  os_boot_storage_fn mount_storage;
  time_t clock_now;
  time_t clock_compiled;
} os_boot_args_t;

/* storage, clock, notify, sensors, then on_load for each installed app. */
int os_boot_load(const os_boot_args_t *args);

#ifdef __cplusplus
}
#endif
