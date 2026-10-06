#include "clock.h"

#include <stdio.h>

/* sin(i * 6 degrees) in Q15. Index 15 is +32767 (3 o'clock). */
static const int16_t k_sin60[60] = {
    0,     3425,  6813,  10126, 13328, 16383, 19260, 21925, 24351, 26509, 28377, 29934,
    31163, 32051, 32587, 32767, 32587, 32051, 31163, 29934, 28377, 26509, 24351, 21925,
    19260, 16383, 13328, 10126, 6813,  3425,  0,     -3425, -6813, -10126, -13328, -16383,
    -19260, -21925, -24351, -26509, -28377, -29934, -31163, -32051, -32587, -32767, -32587,
    -32051, -31163, -29934, -28377, -26509, -24351, -21925, -19260, -16383, -13328, -10126,
    -6813, -3425,
};

static int wrap_unit(int value, int mod) {
    int v = value % mod;
    if (v < 0) {
        v += mod;
    }
    return v;
}

void clock_hms(uint32_t seconds, int* hour, int* minute, int* second) {
    uint32_t tod = seconds % 86400u;
    if (second) {
        *second = (int)(tod % 60u);
    }
    if (minute) {
        *minute = (int)((tod / 60u) % 60u);
    }
    if (hour) {
        *hour = (int)(tod / 3600u);
    }
}

int clock_fmt_hms(char* buf, size_t cap, int hour, int minute, int second) {
    int n;
    if (!buf || cap < 9) {
        return -1;
    }
    hour = wrap_unit(hour, 24);
    minute = wrap_unit(minute, 60);
    second = wrap_unit(second, 60);
    n = snprintf(buf, cap, "%02d:%02d:%02d", hour, minute, second);
    if (n < 0 || (size_t)n >= cap) {
        return -1;
    }
    return n;
}

int clock_hour_sixtieths(int hour, int minute) {
    return wrap_unit(hour, 12) * 5 + wrap_unit(minute, 60) / 12;
}

int clock_minute_sixtieths(int minute) {
    return wrap_unit(minute, 60);
}

int clock_second_sixtieths(int second) {
    return wrap_unit(second, 60);
}

void clock_hand_end(int cx, int cy, int length, int sixtieths, int* x, int* y) {
    int i = wrap_unit(sixtieths, 60);
    int s = k_sin60[i];
    int c = k_sin60[(i + 15) % 60];
    if (x) {
        *x = cx + (int)(((int32_t)s * length) / 32767);
    }
    if (y) {
        *y = cy - (int)(((int32_t)c * length) / 32767);
    }
}
