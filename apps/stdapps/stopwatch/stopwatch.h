#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    bool running;
} stopwatch_t;

void stopwatch_reset(stopwatch_t* sw);

/** Advance seconds. Returns true when minutes should tick (wrap 59→0). */
bool stopwatch_tick_second(stopwatch_t* sw);

/** Advance minutes. Returns true when hours should tick (wrap 59→0). */
bool stopwatch_tick_minute(stopwatch_t* sw);

/** Advance hours (wraps 99→0). */
void stopwatch_tick_hour(stopwatch_t* sw);

#ifdef __cplusplus
}
#endif
