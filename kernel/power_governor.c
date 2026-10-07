#include "power.h"

#include <string.h>

#define POWER_WINDOW_MS 100u
#define POWER_QUIET_WINDOWS 3u
#define POWER_SUSTAINED_WINDOWS 3u
#define POWER_FULL_PERCENT 80u
#define POWER_TEMP_HOT_C 70
#define POWER_TEMP_RECOVER_C 60
#define POWER_LIGHT_SLEEP_MIN_MS 50u

static struct {
    power_level_t level;
    power_demand_t foreground;
    uint32_t busy;
    uint32_t idle;
    uint32_t busy_total;
    uint32_t idle_total;
    power_demand_t window_demand;
    bool saw_busy_demand;
    uint32_t quiet_streak;
    uint32_t full_streak;
    bool sustained_hold;
    power_level_t held_level;
    bool thermal;
    bool have_window;
    uint32_t window_started_ms;
    uint32_t light_sleep_ms;
    power_set_freq_fn set_freq;
    power_set_brightness_fn set_brightness;
    power_read_temp_fn read_temp;
} g;

static power_level_t ceiling_for(power_demand_t demand) {
    if (demand >= POWER_DEMAND_HIGH) {
        return POWER_LEVEL_PERFORMANCE;
    }
    if (demand >= POWER_DEMAND_NORMAL) {
        return POWER_LEVEL_PERFORMANCE;
    }
    return POWER_LEVEL_ECONOMY;
}

static uint32_t freq_for_level(power_level_t level) {
    uint32_t freqs[8];
    int n = power_get_available_freqs(freqs, 8);
    int i;
    int idx;

    if (n <= 0) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        int j;
        for (j = i + 1; j < n; j++) {
            if (freqs[j] > freqs[i]) {
                uint32_t tmp = freqs[i];
                freqs[i] = freqs[j];
                freqs[j] = tmp;
            }
        }
    }

    switch (level) {
        case POWER_LEVEL_PERFORMANCE:
            idx = 0;
            break;
        case POWER_LEVEL_BALANCED:
            idx = n / 2;
            break;
        case POWER_LEVEL_ECONOMY:
            idx = (n >= 2) ? (n - 2) : (n - 1);
            break;
        case POWER_LEVEL_COOL:
        default:
            idx = n - 1;
            break;
    }
    return freqs[idx];
}

static uint8_t brightness_for_level(power_level_t level) {
    switch (level) {
        case POWER_LEVEL_PERFORMANCE:
            return 255;
        case POWER_LEVEL_BALANCED:
            return 180;
        case POWER_LEVEL_ECONOMY:
            return 64;
        case POWER_LEVEL_COOL:
        default:
            return 16;
    }
}

static void apply_level(void) {
    uint32_t mhz = freq_for_level(g.level);
    if (mhz != 0) {
        if (g.set_freq) {
            (void)g.set_freq(mhz);
        } else {
            (void)power_set_cpu_freq(mhz);
        }
    }
    if (g.set_brightness) {
        (void)g.set_brightness(brightness_for_level(g.level));
    }
}

static power_demand_t effective_demand(void) {
    power_demand_t hint = g.foreground;
    if (g.saw_busy_demand && g.window_demand > hint) {
        hint = g.window_demand;
    }
    if (hint == POWER_DEMAND_UNSET) {
        hint = POWER_DEMAND_NORMAL;
    }
    return hint;
}

static bool sample_temp(int* temp_c) {
    int32_t t = 0;
    if (!g.read_temp || g.read_temp(&t) != 0) {
        return false;
    }
    *temp_c = (int)t;
    return true;
}

static void choose_level(power_demand_t hint, bool quiet, bool full) {
    power_level_t ceiling = ceiling_for(hint);

    if (g.thermal) {
        g.sustained_hold = false;
        g.level = POWER_LEVEL_COOL;
        return;
    }

    if (g.level > ceiling) {
        g.level = ceiling;
    }

    if (g.sustained_hold) {
        if (!quiet) {
            if (g.held_level > ceiling) {
                g.held_level = ceiling;
            }
            g.level = g.held_level;
            return;
        }
        g.sustained_hold = false;
    }

    if (quiet) {
        g.full_streak = 0;
        g.quiet_streak++;
        if (g.quiet_streak >= POWER_QUIET_WINDOWS && g.level > POWER_LEVEL_ECONOMY) {
            g.level = (power_level_t)(g.level - 1);
            g.quiet_streak = 0;
        }
    } else {
        g.quiet_streak = 0;
        if (full && hint < POWER_DEMAND_HIGH) {
            g.full_streak++;
            if (g.full_streak >= POWER_SUSTAINED_WINDOWS) {
                g.sustained_hold = true;
                g.held_level = ceiling;
                if (g.held_level > POWER_LEVEL_ECONOMY) {
                    g.held_level = (power_level_t)(g.held_level - 1);
                }
                g.level = g.held_level;
                g.full_streak = 0;
                return;
            }
        } else {
            g.full_streak = 0;
        }
        if (g.level < ceiling) {
            g.level = (power_level_t)(g.level + 1);
        }
    }

    if (g.level > ceiling) {
        g.level = ceiling;
    }
}

static void close_window(uint32_t next_wake_ms, bool any_runnable) {
    uint32_t total = g.busy + g.idle;
    bool quiet = (g.busy == 0);
    uint32_t pct = total ? (g.busy * 100u) / total : 0;
    bool full = !quiet && pct >= POWER_FULL_PERCENT;
    power_demand_t hint = effective_demand();
    int temp_c = 0;

    if (g.thermal) {
        if (sample_temp(&temp_c) && temp_c <= POWER_TEMP_RECOVER_C) {
            g.thermal = false;
        }
    }
    if (!g.thermal && sample_temp(&temp_c) && temp_c >= POWER_TEMP_HOT_C) {
        g.thermal = true;
    }

    choose_level(hint, quiet, full);
    apply_level();

    g.light_sleep_ms = 0;
    if (!any_runnable && (g.level == POWER_LEVEL_ECONOMY || g.level == POWER_LEVEL_COOL) &&
        next_wake_ms >= POWER_LIGHT_SLEEP_MIN_MS && next_wake_ms != UINT32_MAX) {
        g.light_sleep_ms = next_wake_ms;
    }

    g.busy = 0;
    g.idle = 0;
    g.saw_busy_demand = false;
    g.window_demand = POWER_DEMAND_UNSET;
}

void power_governor_reset(void) {
    power_set_freq_fn freq = g.set_freq;
    power_set_brightness_fn bright = g.set_brightness;
    power_read_temp_fn temp = g.read_temp;

    memset(&g, 0, sizeof(g));
    g.level = POWER_LEVEL_BALANCED;
    g.foreground = POWER_DEMAND_NORMAL;
    g.set_freq = freq;
    g.set_brightness = bright;
    g.read_temp = temp;
}

void power_governor_set_foreground_demand(power_demand_t demand) {
    if (demand == POWER_DEMAND_UNSET || demand > POWER_DEMAND_HIGH) {
        demand = POWER_DEMAND_NORMAL;
    }
    g.foreground = demand;
}

power_demand_t power_governor_get_foreground_demand(void) {
    return g.foreground;
}

void power_governor_set_actuators(power_set_freq_fn set_freq, power_set_brightness_fn set_brightness,
                                  power_read_temp_fn read_temp) {
    g.set_freq = set_freq;
    g.set_brightness = set_brightness;
    g.read_temp = read_temp;
}

int power_set_demand(task_tcb_t* task, power_demand_t demand) {
    if (!task || demand == POWER_DEMAND_UNSET || demand > POWER_DEMAND_HIGH) {
        return -1;
    }
    task->power_demand = (uint8_t)demand;
    return 0;
}

power_demand_t power_get_demand(const task_tcb_t* task) {
    if (!task || task->power_demand == POWER_DEMAND_UNSET) {
        return POWER_DEMAND_NORMAL;
    }
    return (power_demand_t)task->power_demand;
}

void power_governor_note_idle(void) {
    g.idle++;
    g.idle_total++;
}

void power_governor_note_busy(task_tcb_t* task) {
    g.busy++;
    g.busy_total++;
    /* A null task only counts as load. It must not invent a NORMAL hint
     * and raise a foreground app that asked for less. */
    if (!task || task->power_demand == POWER_DEMAND_UNSET) {
        return;
    }
    {
        power_demand_t demand = (power_demand_t)task->power_demand;
        if (!g.saw_busy_demand || demand > g.window_demand) {
            g.window_demand = demand;
            g.saw_busy_demand = true;
        }
    }
}

uint32_t power_governor_idle_steps(void) {
    return g.idle_total;
}

uint32_t power_governor_busy_steps(void) {
    return g.busy_total;
}

void power_governor_tick(uint32_t now_ms, uint32_t next_wake_ms, bool any_runnable) {
    if (!g.have_window) {
        g.have_window = true;
        g.window_started_ms = now_ms;
        close_window(next_wake_ms, any_runnable);
        return;
    }
    if ((now_ms - g.window_started_ms) < POWER_WINDOW_MS) {
        return;
    }
    g.window_started_ms = now_ms;
    close_window(next_wake_ms, any_runnable);
}

power_level_t power_governor_get_level(void) {
    return g.level;
}

uint32_t power_governor_light_sleep_ms(void) {
    return g.light_sleep_ms;
}
