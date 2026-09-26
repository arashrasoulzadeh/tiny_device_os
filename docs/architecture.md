# Architecture

See `PLAN.md` for the full design rationale, locked decisions, and phased roadmap.
See `docs/agent-guide.md` for what is implemented versus still planned, and for
the steps to add an app, a HAL function, or a unit test. This file is a short
map of the codebase for orientation; keep it in sync per rule 10 in `PLAN.md`.

## Layers

```
apps/       user-facing apps (shell, filemgr, settings, ota)
game/       ECS, renderer, audio, scripting, assets
modules/    dynamic .ardmod loader
drivers/    display, sensor, ... (probe/remove/open/read/write/ioctl)
fs/         unified VFS over LittleFS (flash) + FatFS (SD)
hal/        hardware abstraction interfaces (include/) + per-arch impls (arch/)
kernel/     scheduler, time, allocator
```

Portable code (kernel, drivers, fs, game, apps) talks to hardware only through the
`hal_*.h` interfaces in `hal/include/`. Each target — `sim`, `esp32`, `esp8266`, `avr`, `rp2040` —
provides its own implementation under `hal/arch/<target>/` or `sim/`.

## Targets

Selected via CMake options in the top-level `CMakeLists.txt`, resolved in
`cmake/toolchain.cmake`:

| Option | Target | Toolchain |
|---|---|---|
| `ARDUBOT_BUILD_SIM` | Host simulator (SDL2 + PortAudio) | native compiler |
| `ARDUBOT_BUILD_ESP32` | ESP32 | ESP-IDF or bare-metal Xtensa GCC |
| `ARDUBOT_BUILD_ESP8266` | ESP8266 | Xtensa lx106 GCC |
| `ARDUBOT_BUILD_AVR` | Mega2560 | avr-gcc |

The simulator is the primary development target: `sim/` implements the HAL against SDL2
(video/GPIO keys), PortAudio (audio), and per-device register models (`sim_i2c.c`,
`sim_spi.c`, `sim/models/`) so board-level drivers can be developed and tested on a host
machine before hardware is involved.

## Testing

- `tests/unit/` — fast, host-only, no simulator init (Unity framework).
- `tests/integration/` — exercises the simulator end-to-end.
- `tests/hardware/` — QEMU + physical hardware (later phases).

CI (`.github/workflows/sim.yml`) builds and runs `tests/unit` + `tests/integration` via the
simulator's `--headless --test=all` mode on Ubuntu, Windows, and macOS on every push/PR.
