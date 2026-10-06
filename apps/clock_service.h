#pragma once

/**
 * Clock service.
 *
 * Keeps the wall clock in /flash/clock.dat through the VFS. A saved time
 * is installed when the board clock is unset. If the file is missing, the
 * compile-time stamp (or the running clock) is written there. Adjustments
 * and a once-a-minute checkpoint rewrite the file so the chip keeps the
 * last known time across a reboot.
 */

#include "os_clock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLOCK_SERVICE_PATH "/flash/clock.dat"

/** Load or create the stored clock and install it when `rtc_now` is unset. */
int clock_service_start(time_t rtc_now, time_t compiled_unix);

/** Shift the clock and write the new time to storage. */
int clock_service_shift(int32_t delta_s);

/** Rewrite storage when the clock is at least a minute ahead of the last save. */
int clock_service_checkpoint(void);

#ifdef __cplusplus
}
#endif
