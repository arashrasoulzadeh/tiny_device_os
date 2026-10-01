#include "stopwatch.h"

void stopwatch_reset(stopwatch_t* sw) {
    if (!sw) {
        return;
    }
    sw->hours = 0;
    sw->minutes = 0;
    sw->seconds = 0;
    sw->running = false;
}

bool stopwatch_tick_second(stopwatch_t* sw) {
    if (!sw || !sw->running) {
        return false;
    }
    sw->seconds++;
    if (sw->seconds >= 60) {
        sw->seconds = 0;
        return true;
    }
    return false;
}

bool stopwatch_tick_minute(stopwatch_t* sw) {
    if (!sw || !sw->running) {
        return false;
    }
    sw->minutes++;
    if (sw->minutes >= 60) {
        sw->minutes = 0;
        return true;
    }
    return false;
}

void stopwatch_tick_hour(stopwatch_t* sw) {
    if (!sw || !sw->running) {
        return;
    }
    sw->hours++;
    if (sw->hours >= 100) {
        sw->hours = 0;
    }
}