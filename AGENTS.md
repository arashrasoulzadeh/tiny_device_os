# AGENTS.md

Instructions for AI coding agents (Claude Code, Cursor, Codex, etc.) working in this repo.
See `PLAN.md` for the full architecture, phased roadmap, and locked design decisions.

## What this project is

ArdubotOS — a multi-architecture embedded OS for Arduino-class devices (AVR Mega2560,
ESP8266, ESP32), developed against a host SDL2 simulator first. C11, CMake, TDD-mandatory.

## Build & test

```bash
# Configure + build the host simulator (default target)
cmake -B build -DARDUBOT_BUILD_SIM=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# Run the full test suite headless (what CI runs)
cd build
./sim/ardubot-sim --headless --test=all --junit=results.xml --coverage=coverage.info

# Or via ctest
ctest --test-dir build --output-on-failure
```

Host dependencies: SDL2, PortAudio.
- macOS: `brew install sdl2 portaudio`
- Linux: `apt-get install libsdl2-dev portaudio19-dev`
- Windows CI: vcpkg (`sdl2`, `portaudio`) + `CMAKE_TOOLCHAIN_FILE` — see `.github/workflows/sim.yml`.

Cross-target builds use `-DARDUBOT_BUILD_ESP32=ON` / `_ESP8266` / `_AVR` instead of `_SIM`
(see `cmake/toolchain.cmake`); these require the relevant toolchains and are not exercised
by this simulator-first workflow unless you're specifically working on a board target.

## Non-negotiable rules

1. **TDD is enforced, not a suggestion.** `scripts/tdd_check.py` runs as a pre-commit hook
   and blocks commits that add implementation code without a corresponding test. Write the
   failing test first, then make it pass.
2. **Warnings are errors.** The build uses `-Wall -Wextra -Wpedantic -Werror` on GCC/Clang.
   A function parameter you don't use still needs `(void)param;` — don't just drop the
   warning class or silence it repo-wide.
3. **Cross-platform by default.** CI runs Ubuntu, Windows, and macOS on every push/PR
   (`.github/workflows/sim.yml`). A change that only builds on your host isn't done.
4. **SOLID / HAL boundaries.** Hardware access goes through `hal/include/hal_*.h`
   interfaces; simulator implementations live in `sim/`, real hardware in `boards/` +
   `hal/arch/`. Don't reach around the HAL from portable code.
5. **DRY.** Shared logic belongs in a common helper, not copy-pasted across board variants
   or sim device models.
6. **Format before commit.** `clang-format` (C/H) and `cmake-format` (CMake) run via
   pre-commit; match the existing style rather than hand-formatting.
7. **Documentation.** When you add or change a public API (a `hal_*.h` header, a kernel
   syscall, a CLI flag, a CMake option), update the relevant doc in `docs/` and this file
   (or `PLAN.md` if it's an architecture-level decision) in the same change. Don't leave
   behavior undocumented for the next agent or human to rediscover.

## Where things live

- `kernel/` — scheduler, time, allocator (target-independent).
- `hal/` — hardware abstraction interfaces (`include/`) and per-arch implementations (`arch/`).
- `sim/` — SDL2/PortAudio-backed simulator: video, audio, storage, net, and per-device
  models (I2C/SPI register simulation, e.g. `sim_i2c.c`, `sim_spi.c`).
- `drivers/`, `modules/`, `fs/`, `game/`, `apps/` — higher-level subsystems, most still
  scaffolding per `PLAN.md`'s roadmap.
- `tests/unit/` — fast host-only tests (Unity framework, no simulator init required).
- `tests/integration/` — tests that exercise the simulator end-to-end.
- `boards/` — per-board linker scripts, startup code, board-specific config.
- `cmake/toolchain.cmake` — per-target (sim/ESP32/ESP8266/AVR) compiler & dependency setup.
- `docs/` — architecture and subsystem documentation.

## CI

`.github/workflows/sim.yml` builds and runs the full test suite on Ubuntu, Windows, and
macOS on every push/PR to `main`/`develop`. If you touch CMake, the toolchain file, or CI
config, check all three jobs mentally (or actually) before assuming it's fixed — a fix
verified on one OS's compiler flags doesn't guarantee the others build clean.
