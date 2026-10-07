# Agent guide

Read this before changing code. `AGENTS.md` is the rule list. `PLAN.md` is the
locked design and the multi-month roadmap — much of it is not implemented yet.
This file is the map of the code that exists today.

## What is real vs planned

Implemented and exercised by host tests:

- Cooperative scheduler (`kernel/scheduler.c`): up to 16 tasks, priority ready
  lists, `task_sleep`, host fibers via `kernel/host_stack.c`.
- Time and a bump allocator (`kernel/os_time.c`, `kernel/alloc.c`). Wall
  clock is `kernel/os_clock.h`. `apps/clock_service.c` stores it in
  `/flash/clock.dat` through the VFS and restores it when the board clock
  is unset. `apps/sensor_service.c` registers sensors from the device
  config (`key`, `type` `temp` or `adc`, `refresh_ms`) and
  `app_helper_sensor()` reads one by key. `apps/stdapps/sensors/` lists
  every registered sensor. `apps/notify_service.c` is the notification
  card. It is not an app: `stdapps_install()` does not register it and it
  never takes focus. `notify_post()` queues a title and optional body that
  slides over the focused app, either full-screen or as a top band. A
  button press dismisses it; otherwise it leaves three seconds after the
  show animation. Covered by `tests/unit/test_notify_service.c`.
  `link/` is the host/device session (frame, AES-128-GCM, four calls, plus
  channel `LINK_CH_TTY`). `apps/link_bridge.c` delivers the calls on the
  device: `notify.post` to `notify_post()`, `app.message` into a queue
  (`link_message_take`), `app.event` onto `app.<name>.<event>`,
  `input.key` through `sim_gpio_handle_key()`. `apps/tty_service.c` is the
  USB shell, started when Hello succeeds. It is not the panel app in
  `apps/stdapps/shell/`. Sensor samples are not pushed; `sensors` and
  `sensor <key>` read them. `apps`, `switch`, `fg`, `run`, `kill`, and
  `key` control installed apps. `run` uses a text hook when one is
  registered (`tty_cli_register`); otherwise it focuses the app and
  attaches the keyboard. The Python SDK is `scripts/link_codec.py`
  (`Link.open_usb`, `Link.open_tcp`, `Link.call`, `Link.send_tty`). When
  `notifications.forward_from_host` is set, `make usb` stays up after a
  successful flash as a raw terminal (`scripts/link_monitor.py`) until
  Ctrl+]. Ctrl+Z and Ctrl+C go to the shell. The 16-byte key is `link.key`
  in `device_secrets.yaml`, not the device config.
- HAL interfaces in `hal/include/hal_*.h` with a working **sim** backend in
  `hal/arch/sim/` plus SDL/device models in `sim/`.
- Board backends under `hal/arch/{esp32,esp8266,avr}/` exist as per-arch files;
  they are not the day-to-day development target.
- App runtime: manifests, **`app_kit`** (`APP_DEFINE`, focus, key bind, open/exit),
  plus UI components under `apps/ui/components/` (`canvas`, `screen`, `menu`,
  `catalog`, `icons`, `status`, `display`). `apps/header_app.c` is the
  status band (battery and clock) on every standard app, including the
  launcher. `.game` and `.fullscreen` omit it. Guides: `docs/appkit.md`,
  `docs/apps.md`.
- Built-in apps under `apps/stdapps/<name>/`: `launcher`, `counter`, `info`,
  `stopwatch` (three worker tasks: sec/min/hour), `pong` (Up/Down paddle),
  `widgets`, `pomodoro`, `taskmgr`, `clock`, `sensors`, plus `settings`,
  `fileman`, `shell`, and `demo`. The last four are compiled into the
  simulator and are not installed, so the launcher does not list them.
  `stdapps_install()` is the only install list. `stdapps_start_name()` boots
  sensors, then info, clock, pomodoro, or the launcher. The sensors app also
  lists CPU busy percent, heap used percent, and the CPU clock (`pwr`, in MHz).
- VFS (`fs/vfs.c`) with LittleFS/FatFS backends (`fs/littlefs/`, `fs/fatfs/`),
  config KV store (`fs/config_store.c`), and OTA with A/B partitions + ed25519
  signature verification (`fs/ota.c`, `fs/ed25519.c`).
- Power management (`kernel/power.c`, `kernel/power_governor.c`) — sleep
  modes, wake sources, CPU frequency scaling, per-driver suspend/resume,
  and an OS-wide governor. Apps publish a demand hint; the governor is the
  only writer of the clock and the backlight cap. See `docs/power.md`.
  Covered by `tests/unit/test_power.c` and `tests/unit/test_power_governor.c`.
  The tickless-idle path in `kernel/scheduler.c`
  (`scheduler_enter_idle`/`scheduler_tickless_idle`/`scheduler_exit_idle`)
  is covered directly in `tests/unit/test_scheduler.c`. `idle_task()` still
  never runs, because `scheduler_step()`/`task_yield()` skip `TASK_PRIO_IDLE`;
  those skipped steps are what the governor counts as idle. Automatic deep
  sleep is not part of the governor.
- Driver framework (`drivers/`) — core (`driver.c`, open-handle dispatch,
  owned-vs-caller-owned device lifetime), the device registry with I2C/SPI
  hotplug scan (`device_registry.c`), the dynamic `.ardmod` module loader
  (`module.c`, CRC-verified headers, symbol table and dependency resolution
  still stubs), and all six built-in bus drivers (GPIO/I2C/SPI/UART/WiFi/
  display) against their sim HAL backends. Covered by `tests/unit/test_driver.c`,
  `test_device_registry.c`, `test_module.c`, `test_bus_drivers.c`.
- Simulator CLI: headless mode, `--test=all`, JUnit, coverage
  (`sim/sim_main.c`, `sim/sim_args.c`). The sim main loop advances
  **one scheduler tick per wall-clock millisecond** (catch-up when a frame
  takes longer than 1 ms), so `task_sleep(1000)` is ~1 real second.
  Interactive sim uses a **320×172** panel (the ESP32-C6 LCD). The NodeMCU
  profile in `device_config.yaml` stays **128×32** with Up/Select (long
  Select = back).

Still roadmap (do not assume these work, and do not invent them while fixing
something else): game engine, Lua/WASM, dynamic `.ardmod` loading as a product,
Arduino IDE packaging, deep-sleep power targets, `tests/hardware/`.

`fs/fatfs/fatfs_vfs.c` is a flat raw-storage passthrough, not a real FAT
filesystem: no FAT table, no directory entries, two different paths opened
at the same time alias the same underlying bytes. See the comment at the
top of that file before extending it - there's no per-file state to extend,
it needs replacing when a real SD card (PlatformIO FatFs) lands.

All host tests in `ctest --test-dir build` are the suite. The build is
warning-clean (`-Wall -Wextra -Wpedantic -Werror`). If you hit a failure or
warning that looks unrelated to your change, it is a regression - root-cause
it rather than assuming it is "pre-existing."

## Dependency direction

Portable code calls hardware only through `hal_*.h`.

```
apps/  →  kernel/, fs/, drivers/, hal/include/
drivers/  →  hal/include/
sim/ + hal/arch/*  →  implement hal/include/
```

`apps/app_framework.h` also includes sim headers (`sim_gpio.h`, device models).
That coupling is current reality. New portable APIs still belong on the HAL,
not as extra includes of `sim/` from kernel or drivers.

## Build and test (host only)

```bash
cmake -B build -DARDUBOT_BUILD_SIM=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Or via the Makefile wrapper: `make`, `make test`, `make run` (SDL emulator).

`make compile-commands` writes the repo-root `compile_commands.json` that clangd and
Cursor use for completions and Find All References. It merges the simulator
database with PlatformIO's `esp32-c6`, `esp32-c6-kernel`, and `nodemcu` databases
so board sketches are indexed too. Reload the editor window after generating it.

Wi-Fi icons in the emulator use `device_secrets.yaml` when that file exists
(copy `device_secrets.yaml.example`). Without it the status bar shows Wi-Fi off.
See `README.md`.

### USB / hardware (`make usb` → PlatformIO)

`device_config.yaml` at the repo root describes the connected board (arch, serial
port, LCD, buttons, `pio_env`). `device_secrets.yaml` (gitignored) holds
`wifi.ssid` and `wifi.password`; `make usb` will not flash hardware until both
are set. `make usb` asks which target to compile for (unless `DEVICE=` is set),
generates `build/generated/device_config.h` and `device_secrets.h`, then builds
and flashes with [PlatformIO](https://platformio.org/) (`platformio run -e nodemcu -t upload`).

```bash
make run                      # SDL2 emulator
make device-config            # validate YAML + emit headers
make usb-ports                # list /dev/cu.usb* etc.
make usb                      # interactive → PlatformIO build + upload
make usb DEVICE=nodemcu PORT=/dev/cu.wchusbserial1410
python3 tests/unit/test_device_config.py
python3 tests/unit/test_device_secrets.py
```

Files: `platformio.ini`. Builtin apps are only `apps/stdapps/`; board
entry points install them through `apps/stdapps_register.c`. The ESP32-C6
image (`[env:esp32-c6]`) boots that set. Its ST7789 path composites into
`hal/display_fb.c` and presents dirty rows from `hal_display_flush()`
(called by `app_display_flush` / `app_ui_end_frame`) so a clear-then-draw
is not visible on the glass. SPI stays at 40 MHz; the panel scan stays at
the driver's ~60 Hz. The NodeMCU image does not host
the app framework yet, so it does not draw its own launcher. Wiring:
NodeMCU SSD1306 128×32 I2C (SCL=D1, SDA=D2), **UP** on D5, **SELECT** on D6
(other side GND; long-press SELECT = back to launcher).

On Apple Silicon, ESP8266 builds need Rosetta (`Bad CPU type in executable` means
it is missing): `softwareupdate --install-rosetta --agree-to-license`.

NodeMCU upload uses `scripts/nodemcu_upload.py` (Arduino-style DTR/RTS auto-reset)
so you should not need to hold FLASH; CH340 port drops during reset are retried.

One unit test binary:

```bash
cmake --build build -j --target test_scheduler
./build/tests/unit/test_scheduler
```

`tests/unit/CMakeLists.txt` globs `test_*.c`. A new `tests/unit/test_foo.c`
is picked up on the next CMake configure. Re-run `cmake -B build ...` if the
new file is not a target yet.

`test_headless_exit` and `test_sim_args` link `sim_args` instead of the `sim`
library so they do not pull a second `main`. Follow that if a test must call
argument parsing without the simulator entry point.

## TDD gate

`scripts/tdd_check.py` (pre-commit) rejects a staged `.c` file outside `tests/`
unless one of these already exists and contains `RUN_TEST`, `TEST_ASSERT`,
`void test_`, `UNITY_BEGIN`, or `UNITY_END`:

- `tests/unit/test_<stem>.c`
- `tests/unit/test_<parent>_<stem>.c`
- `tests/integration/test_<stem>.c`

Write that test file before the implementation file. Do not bypass the hook.

Unity tests look like the files already in `tests/unit/`: `setUp`/`tearDown`,
`void test_...`, `RUN_TEST`, `UNITY_BEGIN`/`UNITY_END`, return `UNITY_END()`.

Warnings are errors (`-Wall -Wextra -Wpedantic -Werror`). Unused parameters need
`(void)param;`.

## Add a built-in app

Use **`app_framework.h`** and `APP_HELPER` for a new screen — see [`docs/apps.md`](apps.md)
and `apps/stdapps/counter/counter_app.c`. Pass `.on_view` for a scene,
`.every_ms` for a timer, `.keys` for a custom map, `.on_ready` to start a
worker, `.live` when the screen must repaint every tick, and `.state` /
`.state_size` for the session restored on start and resume and stored on quit.
`app_kit.h` / `APP_DEFINE` is the lower-level path when those fields are not enough.
Files and pins are `fw/io.h`.

1. Create `apps/stdapps/<name>/` and implement `on_event` / `on_view` (and
   optional `on_ready`, `on_tick`, `on_cleanup`) in `<name>_app.c`. Use
   `on_draw` when the screen places its own pixels.
2. Fill the scene with `app_scene_row`, `app_scene_hero`, `app_scene_clock`,
   `app_scene_bar`, `app_scene_gauge`, or `app_scene_panel`.
3. End the file with
   `APP_HELPER(my_app, "my_app", .on_event = ..., .on_view = ...)`.
   Put name, version, author, description, title, and help in `app.json`;
   the compile reads that file into the manifest. The icon is `my_app_icon`.
   Leave `.type` off unless the app is not a tool. `.fps` applies only with
   `.live` or `.game`.
4. Add the directory to `apps/stdapps/CMakeLists.txt` (`create-app` does this).
   To install it, add an `ARDUBOT_APP_<NAME>_ENABLED` block inside
   `stdapps_install()` in `apps/stdapps_register.c`. Boot calls
   `stdapps_start_name()`, not a name hardcoded in `sim/sim_main.c`.

Lower-level helpers remain in `apps/app_framework.h` if you need them.

## Add a HAL operation

1. Declare it on the matching `hal/include/hal_*.h` (opaque handle, open/close,
   suspend/resume already exist on GPIO — match that style).
2. Implement it in `hal/arch/sim/` first. That is what tests run.
3. Add a `tests/unit/test_hal_<name>_sim.c` (or extend the existing one) before
   the new `.c` body, so the TDD hook is satisfied.
4. Stub or implement the same symbol on esp32, esp8266, and avr if the header
   is part of the common HAL. A missing symbol breaks those targets even when
   sim CI is green.

## Scheduler constraints

- `MAX_TASKS` is 16. Names are `TASK_NAME_LEN` (16) bytes.
- Host context switches use `host_fiber` on the TCB. Bare-metal fields
  (`jmp_buf`, `stack_ptr`) are separate. Do not assume `setjmp` is the sim path.
- Public entry type is `task_entry_t` (`void (*)(void* arg)`), declared in
  `kernel/scheduler.h`.

## Docs to update in the same change

| Change | Update |
|---|---|
| Public HAL, syscall, CLI flag, CMake option | `docs/` and, if agents need a new command, `AGENTS.md` |
| Locked architecture or phase scope | `PLAN.md` |
| "Where is this code and how do I extend it" | this file |

Leave `PLAN.md` phase tables alone unless the user is changing the roadmap.
