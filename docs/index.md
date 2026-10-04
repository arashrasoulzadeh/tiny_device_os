# ArdubotOS

**A multi-architecture embedded OS for AVR, ESP8266, and ESP32 — built
simulator-first.** The same app runtime that ships to a real board runs
inside an SDL2 host emulator, so apps are written, run, and tested without
hardware in the loop.

**[Get started](#get-started)** · **[Write your first app](tutorials/writing-an-app.md)** · **[See the architecture](architecture.md)**

---

## Why this exists

Most embedded projects force a choice: flash real hardware for every
iteration, or fake enough of the platform that "works in the simulator"
stops meaning anything. ArdubotOS's answer is a HAL boundary strict enough
that `sim/` and `hal/arch/esp32/` are interchangeable at build time — the
cooperative scheduler, the VFS, the driver framework, and every stdapp run
unmodified against both. Current real hardware target: a NodeMCU (ESP8266)
with a 128×32 SSD1306 OLED.

## At a glance

| | |
|---|---|
| **Tests** | 45/45 passing (`ctest --test-dir build`) |
| **Coverage** | 55.0% line / 59.9% function across 59 host-buildable source files |
| **Build** | Warning-clean (`-Wall -Wextra -Wpedantic -Werror`) |
| **Targets** | sim (SDL2) · ESP32 · ESP8266 · AVR (Mega2560) |
| **Language** | C11, CMake |
| **Discipline** | TDD enforced by a pre-commit hook (`scripts/tdd_check.py`) |

## What's actually implemented

Not a wishlist — every item below is exercised by the host test suite today.
See [the agent guide](agent-guide.md) for the full, honest breakdown of what's
real versus still roadmap.

- **Cooperative scheduler** — `setjmp`/`longjmp` host fibers, priority ready
  lists, tickless idle, up to 16 tasks.
- **TLSF memory allocator** — O(1) malloc/free, pool-based, built for a
  fixed-size microcontroller heap.
- **Power management** — sleep modes, wake sources, CPU frequency scaling,
  per-driver suspend/resume.
- **VFS** — unified `open/read/write/seek/stat` over LittleFS (`/flash`) and
  FatFS (`/sd`), a config KV store, and A/B OTA with signature verification.
- **Driver framework** — `probe/open/read/write/ioctl`, a device registry
  with I2C/SPI hotplug scan, and a dynamic `.ardmod` module loader.
- **App runtime** — `app_kit`, focus/key-bind model, a built-in launcher and
  eight stdapps (shell, file manager, settings, pong, stopwatch, ...).

## Get started

```bash
make run    # build (Debug, sim) and open the SDL2 emulator
make test   # build and run the headless test suite
```

Panel controls match the board: Up/Down move, Select launches, hold
Select/Escape to go back.

New to the codebase? Walk through building one small app end to end —
scaffold, state, a background task, installing it, and a test — in
**[Writing an app](tutorials/writing-an-app.md)**.

## Map of the docs

| Doc | Read it for |
|---|---|
| [Architecture](architecture.md) | The layer stack, the scheduler's boot/switch sequence, and the TLSF allocator — with diagrams |
| [Agent guide](agent-guide.md) | What's real vs. roadmap, build/test commands, the TDD gate, where to add things |
| [App kit](appkit.md) | The full `app_kit`/`app_ui` API reference |
| [Apps](apps.md) | The built-in stdapps and how the launcher boots |
| [Writing an app — tutorial](tutorials/writing-an-app.md) | Build one app from nothing, hands-on |

Locked architecture decisions and the full phased roadmap live in
[`PLAN.md`](https://github.com/arashrasoulzadeh/tiny_device_os/blob/main/PLAN.md)
at the repo root, not here — this site covers "how it works and how to
build on it," `PLAN.md` covers "what's decided and what's next."
