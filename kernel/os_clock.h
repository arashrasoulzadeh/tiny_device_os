#pragma once

/**
 * Wall clock.
 *
 * time_now_ms() is uptime. This is civil time: the host clock in the
 * simulator, and the board clock after os_clock_stamp() is installed at
 * boot. A board with no battery RTC reads unset (near the epoch) until
 * that install. The stamp is the build machine's clock unless device
 * config sets clock.unix.
 */

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** RTC values after this unix second count as already set. */
#define OS_CLOCK_SET_AFTER 1600000000L

/**
 * Unix time to install, or 0 when `rtc_now` is already set or
 * `compiled_unix` is not a real timestamp.
 */
time_t os_clock_stamp(time_t rtc_now, time_t compiled_unix);

/** POSIX TZ for `offset_min` minutes east of UTC. 0 on success, -1 if buf is too small. */
int os_clock_tz_string(int offset_min, char* buf, size_t cap);

/** Current civil time, including any host-side shift from os_clock_shift(). */
time_t os_clock_now(void);

/** Move the clock by `delta_s` seconds. On the host this stays inside the process. */
int os_clock_shift(int32_t delta_s);

/** Set the civil clock to an absolute unix time. On the host this stays inside the process. */
int os_clock_set(time_t unix_time);

/** Drop a host-side shift. The board clock is the RTC, so this is a no-op there. */
void os_clock_reset(void);

#ifdef __cplusplus
}
#endif
