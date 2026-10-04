#pragma once

/* ssize_t is POSIX, not standard C - <sys/types.h> provides it on every
 * platform this project actually builds for (sim on macOS/Linux, the
 * ESP-IDF/Arduino toolchains, avr-gcc) except MSVC, which has no
 * <sys/types.h> definition for it at all. Pulled in by every header that
 * declares a read/write-style API (drivers/driver.h, fs/vfs.h,
 * apps/app_framework.h) instead of duplicating this guard three times. */
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <sys/types.h>
#endif
