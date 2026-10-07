# Power and cooling

There is no fan. Cooling means a slower clock and a dimmer backlight. Saving
power uses the same two controls, plus a light-sleep recommendation when
nothing is runnable.

`kernel/power_governor.c` is the only policy that changes them. Apps do not
pick a megahertz. They publish how much compute they want, and the governor
treats that as a ceiling.

## Levels

The governor ranks whatever frequencies `power_get_available_freqs()`
returns. It does not hardcode 240 MHz. On the host those are 240, 160, 80,
40, and 10. The ESP32-C6 boot path installs 160, 80, 40, 20, and 10.

| Level | When | Clock | Backlight cap |
|---|---|---|---|
| Performance | A short burst, or an app that asked for `HIGH` | Highest | 255 |
| Balanced | Default interactive app | Middle | 180 |
| Economy | Idle, or the app asked for little work | Second lowest | 64 |
| Cool | The die is hot, or a task is burning CPU without having asked for `HIGH` | Lowest | 16 |

One busy window steps **up**, so a tap still feels quick. Several quiet
windows step **down**, so the clock does not flicker. A sustained full load
with a hint below `HIGH` drops one level and stays there until the machine
goes quiet. `HIGH` may keep the top clock.

Temperature, when a sensor answers, overrides everything. At 70 °C the level
is Cool, including a refused `HIGH` request. It stays there until the die
falls to 60 °C. No sample means the thermal rule stays off. The simulator
has no die sensor.

## What apps publish

```c
APP_HELPER(clock_app, "clock",
    .demand = POWER_DEMAND_LOW,
    .every_ms = 100,
    .on_view = on_view);
```

| Hint | Meaning |
|---|---|
| `POWER_DEMAND_LOW` | Timer, clock, sensor list. Cannot sit on the top clock. |
| `POWER_DEMAND_NORMAL` | Default interactive app. |
| `POWER_DEMAND_HIGH` | A game that may use the top clock until the die is hot. |
| `POWER_DEMAND_IDLE` | Nothing runnable should be doing work. |

`app_kit_set_demand()` is the same hint for a screen that does not use
`APP_HELPER`. Leaving the foreground puts that app's live hint back to
`NORMAL`, so a game in the background does not keep the fast clock. Pomodoro,
clock, and sensors publish `LOW` on the app task. A worker that must stay
quiet calls `power_set_demand()` itself.

`scheduler_step()` never runs `idle_task()`. A step that finds only the idle
task is an idle sample. `scheduler_tick()` closes a 100 ms window and calls
`power_governor_tick()`.

## Light sleep, not deep sleep

When the level is Economy or Cool, nothing is runnable, and the next wake is
at least 50 ms away, `power_governor_light_sleep_ms()` returns that gap. The
governor does **not** call `power_light_sleep()`. On the host that function
blocks on the wall clock. Deep sleep stays an explicit `power_deep_sleep()`
call. It drops RAM and is still outside this policy.

## Actuators

Tests install fakes with `power_governor_set_actuators()`. A NULL frequency
callback uses `power_set_cpu_freq()`. A NULL temperature callback leaves the
thermal rule off.

The simulator registers `hal_display_set_backlight_cap()`. That cap clamps
later `hal_display_set_brightness()` calls. The ESP32-C6 backlight is on/off,
so a non-zero cap leaves the panel on and only a zero cap blanks it.

ESP32-C6 boot (`boards/esp32-c6-lcd/src_kernel/kernel_boot.cpp`) registers
`hal_power_set_cpu_freq()` and reads the die through the `temp` sensor the
board config already owns. `hal_power_get_die_temp_c()` is the same reading
for a caller that does not go through the sensor service. AVR and ESP8266
report no die temperature.
