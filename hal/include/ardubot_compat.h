#pragma once

/* ssize_t is POSIX, not standard C - MSVC's own <sys/types.h> provides
 * off_t (as a plain `long`) but not ssize_t, so it still needs including
 * for off_t on MSVC too, not skipping it outright. Pulled in by every
 * header that declares a read/write/seek-style API (drivers/driver.h,
 * fs/vfs.h, apps/app_framework.h) instead of duplicating this guard
 * three times. */
#include <sys/types.h>
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
