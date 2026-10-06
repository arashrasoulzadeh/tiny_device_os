#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* seconds since midnight, wrapped to 24 hours. */
void clock_hms(uint32_t seconds, int* hour, int* minute, int* second);

/* "HH:MM:SS". Out-of-range fields wrap. Returns the length, or -1. */
int clock_fmt_hms(char* buf, size_t cap, int hour, int minute, int second);

/* Position around the face. 0 is 12 o'clock, 15 is 3, counting clockwise
 * in steps of 6 degrees. */
int clock_hour_sixtieths(int hour, int minute);
int clock_minute_sixtieths(int minute);
int clock_second_sixtieths(int second);

/* End of a hand of @p length pixels. 0 points up. */
void clock_hand_end(int cx, int cy, int length, int sixtieths, int* x, int* y);

#ifdef __cplusplus
}
#endif
