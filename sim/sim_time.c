#include "sim_time.h"
#include <SDL2/SDL.h>
#include <stdint.h>
#include <time.h>

static uint64_t g_start_ticks = 0;
static uint64_t g_current_ticks = 0;

int sim_time_init(void) {
    g_start_ticks = SDL_GetPerformanceCounter();
    g_current_ticks = g_start_ticks;
    return 0;
}

void sim_time_cleanup(void) {
}

uint64_t sim_time_now_us(void) {
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t now = SDL_GetPerformanceCounter();
    return ((now - g_start_ticks) * 1000000) / freq;
}

uint32_t sim_time_now_ms(void) {
    return (uint32_t)(sim_time_now_us() / 1000);
}

void sim_time_update(void) {
    g_current_ticks = SDL_GetPerformanceCounter();
}

void sim_time_sleep_ms(uint32_t ms) {
    // Use nanosleep for better precision on Unix systems
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void sim_time_sleep_us(uint32_t us) {
    if (us >= 1000) {
        SDL_Delay(us / 1000);
    }
}