# PRD: real-hardware ArdubotOS (kernel + apps/stdapps) on ESP32-C6 (RISC-V)

Status: **not started — planning only.** Nothing in this document has been
implemented. This is a PRD + phased execution plan, written so each phase
can be picked up independently, in a fresh session, with minimal context
re-derivation (small token budget per phase).

## Problem statement

Two things both call themselves ArdubotOS today and share no code:

1. **The simulator** (`make run ARCH=sim`) — the real kernel (`kernel/`),
   the real app framework (`apps/`, `apps/stdapps/`), built via CMake,
   running on the host.
2. **The ESP32-C6 board's firmware** (`boards/esp32-c6-lcd/src/main.cpp`) —
   a standalone Arduino/PlatformIO sketch that reimplements the same ideas
   (launcher, counter, info, stopwatch, pong, task manager, pomodoro)
   independently.

The physical device only runs (2). This PRD's goal: make the physical
device run (1) — the real kernel + `apps/stdapps/*` — with **one firmware
file**, no separate reimplementation.

## Goal / success criteria

- A single PlatformIO/ESP-IDF build target compiles `kernel/` +
  `apps/app_framework.*` + `apps/stdapps/*` for ESP32-C6 and flashes to the
  physical board.
- `apps/stdapps/launcher`, `counter`, `pomodoro`, `taskmgr`, `pong` all
  render and respond to input identically in spirit to their simulator
  behavior (allowing for the real ST7789 panel vs. the simulator's
  `ssd1306_model`).
- `boards/esp32-c6-lcd/src/main.cpp` (the Arduino sketch) is left
  untouched and still flashable throughout, as a fallback, until the new
  path is proven on real hardware end-to-end.
- Every phase below ends in a state that builds and passes its own tests
  before moving to the next phase, and is pushed to `origin/main` before
  starting the next phase (see "Push discipline").

## Push discipline

**Push after each phase**, once that phase's own build/tests are green.
Do not batch multiple phases into one commit/push — if phase 3 turns out
to be wrong, phases 1-2 should already be safe on `origin/main`.
Each phase's commit message should name the phase (e.g. `feat(riscv):
phase 1 - host_call_on_stack RISC-V branch + QEMU test`).

## TDD approach per phase

Per `AGENTS.md`/`CLAUDE.md`, `scripts/tdd_check.py`'s pre-commit gate is
real — write the failing test first, then the implementation, for each
phase. Each phase below states its test first, explicitly, before its
implementation steps.

---

## Phase 0 — survey & confirm toolchain decision (no code)

**Goal:** resolve the one open architectural question (build system) so
phases 1-4 don't get re-litigated mid-stream.

**Steps:**
1. Check whether `IDF_PATH` (a real ESP-IDF install) is feasible in this
   environment, vs. PlatformIO's `framework = espidf`, vs. a from-scratch
   RISC-V linker script + startup file. Decision drivers:
   - PlatformIO `framework = espidf` expects ESP-IDF *component* layout
     (`idf_component_register`, `CMakeLists.txt` per component) —
     `kernel`/`apps`/`hal` don't use this today. Adopting it means
     restructuring those directories' CMake, which has a blast radius
     across the existing sim build too (shared CMakeLists).
   - A real `IDF_PATH` install sidesteps new linker scripts and startup
     code (ESP-IDF supplies both) but is a manual SDK install, untested
     for network access in this environment.
   - A from-scratch RISC-V linker script + startup `.S` is the most
     self-contained (no new build-system integration) but the most
     manual/error-prone (interrupt vectors, memory map, etc. all written
     by hand).
   - **Recommendation:** try the real `IDF_PATH` install first — it is the
     least invasive to the existing CMake structure (ESP-IDF's own
     toolchain file can be pointed at from `cmake/toolchain.cmake` as a
     new branch, without touching how `kernel`/`apps` are built for sim).
     Fall back to the from-scratch linker/startup route only if the
     ESP-IDF install is impractical in this environment.
2. Record the decision directly in this file's "Decision log" section
   below before starting Phase 1.
3. Push: `docs(riscv): phase 0 - toolchain decision recorded`.

**Decision log:** (fill in during Phase 0, before Phase 1 starts)
- Toolchain chosen: _TBD_
- Why: _TBD_

---

## Phase 1 — `kernel/host_stack.c` RISC-V branch (critical path)

**Status: done, with a known gap.** The `#elif defined(__riscv)` branch is
implemented in `kernel/host_stack.c`. QEMU verification was **not**
performed: the only RISC-V cross-GCC available via Homebrew lives in the
third-party `riscv-software-src/riscv` tap, which requires `brew trust`
(a broad, whole-tap trust grant) to install — the user explicitly chose
to skip this rather than grant that trust. The sim host build (aarch64/
x86_64, unaffected by this branch) still builds clean and all 45 existing
tests pass, including the architecture-generic
`tests/unit/test_host_stack.c`. **Before relying on this branch on real
hardware**, either: get a RISC-V cross-GCC + QEMU from another source
(xpack, a vetted tap, or a manual toolchain build) and run
`test_host_stack.c` under it, or validate directly on the ESP32-C6 in
Phase 2+ and treat any boot failure there as grounds to revisit this
assembly.

**Goal:** `host_call_on_stack()` works on RISC-V — the primitive every
cooperative task's stack switch depends on. Nothing else in this PRD
matters until this is proven correct, independent of any display/board
code.

**Why first:** it's small, self-contained, and testable on the host
(via QEMU) without needing hardware, a display driver, or a toolchain
decision beyond "can I get a RISC-V GCC + QEMU".

**Test first (TDD):**
- Add a host-side test (new file, e.g. `kernel/tests/test_host_stack_riscv.c`
  or extend the existing `host_stack` test if one exists — check
  `docs/agent-guide.md` / existing `kernel/` test layout before naming a
  new one) that:
  1. Allocates a stack buffer.
  2. Calls `host_call_on_stack(stack_top, fn, arg)` with a `fn` that
     writes a known sentinel value through `arg` and never returns via
     normal `return` (matches existing contract: "switch sp, call fn(arg),
     never return").
  3. Asserts the sentinel was written and (if feasible) that `sp` inside
     `fn` was within the provided stack buffer's bounds.
- This test should initially fail to **compile** for RISC-V targets
  (hits the existing `#error "unsupported host architecture"`), which is
  the expected red state before implementation. If CI only builds x86/ARM
  natively, gate this test behind a RISC-V cross-compile + QEMU step (see
  below) rather than breaking the default native build.

**Implementation steps:**
1. Set up a RISC-V cross toolchain + QEMU locally (e.g.
   `riscv64-unknown-elf-gcc`/`riscv32-unknown-elf-gcc` + `qemu-system-riscv32`,
   or whatever package manager is available in this environment — record
   exact packages used in the commit message for reproducibility).
2. Add the `#elif defined(__riscv)` branch to `kernel/host_stack.c`:
   ```c
   #elif defined(__riscv)
   void host_call_on_stack(void* stack_top, void (*fn)(void*), void* arg) {
       uintptr_t sp = ((uintptr_t)stack_top) & ~(uintptr_t)15u; /* 16-byte align */
       __asm__ __volatile__(
           "mv sp, %0\n\t"
           "mv a0, %2\n\t"
           "jalr %1\n\t"
           :
           : "r"(sp), "r"(fn), "r"(arg)
           : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
             "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");
       abort();
   }
   #endif
   ```
3. Cross-compile the test for RISC-V, run under QEMU, confirm it passes
   (sentinel written, no crash, stack pointer in bounds).
4. Wire this into CI only if it can run cheaply/reliably there (a new,
   optional job) — do not block the existing Ubuntu/Windows/macOS sim
   matrix on a RISC-V QEMU dependency unless it's proven stable. If CI
   integration is impractical this phase, note that explicitly and leave
   the test runnable manually via a documented command (add it to
   `docs/agent-guide.md` or a `scripts/` helper).

**Done when:** the QEMU-run test passes; `kernel/host_stack.c` has the new
branch; existing non-RISC-V branches/tests are untouched and still pass.

**Push:** `feat(riscv): phase 1 - host_call_on_stack RISC-V branch + QEMU test`

---

## Phase 2 — toolchain + build skeleton for ESP32-C6

**Goal:** a buildable (not yet functional) PlatformIO/CMake target that
compiles `kernel/` + `apps/app_framework.*` (stubbed display) for the
ESP32-C6, producing a flashable (even if blank-screen) binary. This phase
proves the build/link path before any display code is written.

**Test first (TDD):**
- Since this phase is build-system plumbing, the "test" is the build
  itself succeeding, plus a minimal smoke check: the firmware boots and
  prints something over the native USB-CDC serial port (reusing the
  `ARDUINO_USB_CDC_ON_BOOT=1`/`ARDUINO_USB_MODE=1` flags already proven in
  `platformio.ini`'s `esp32-c6` env). Write this as an explicit manual
  checklist item (flash, open serial monitor, confirm boot banner) since
  it's hardware-in-the-loop and not unit-testable on host — record the
  exact expected output string in the PR/commit so it's reproducible.

**Implementation steps (follow the Phase 0 decision):**
1. New PlatformIO env, e.g. `[env:esp32-c6-kernel]`, separate from the
   existing `[env:esp32-c6]` (which stays pointed at `main.cpp` — do not
   modify that env).
2. Wire the chosen toolchain path (`IDF_PATH`, PlatformIO `espidf`
   framework, or custom linker/startup per Phase 0's decision).
3. Point the new env's build at `kernel/*.c` + `apps/app_framework.c` +
   a minimal `apps/stdapps/` subset (start with zero apps — just prove
   kernel init + scheduler start + idle loop link/boot).
4. Stub `apps/app_framework.h`'s display calls to a no-op real-hardware
   branch for now (actual ST7789 driving is Phase 3) — just enough to
   link.
5. Flash, confirm boot serial output per the smoke check above.

**Done when:** `pio run -e esp32-c6-kernel` succeeds, flashes, and prints
the expected boot banner over serial. No display output expected yet.

**Push:** `feat(riscv): phase 2 - ESP32-C6 kernel build skeleton boots over serial`

---

## Phase 3 — `hal_display_esp32.c` real ST7789 driver

**Goal:** real pixels on the physical panel through the HAL, not the
simulator's `ssd1306_model`.

**Test first (TDD):**
- Host-side: if `hal_display_esp32.c`'s pixel/rect packing logic can be
  isolated from the actual SPI transaction (e.g. a pure function that
  turns `(x,y,color)` into the byte sequence sent to the panel), unit-test
  that logic on the host without hardware.
- Hardware-in-the-loop smoke test (manual, documented): flash, confirm a
  known test pattern (e.g. solid color fill, or a few colored rectangles)
  appears correctly oriented on the real panel — reuse the rotation/offset
  values already proven correct in `boards/esp32-c6-lcd/src/gfx_mono.h`
  (the `(34,0)`/`(0,34)` landscape fix from the Arduino sketch) rather than
  rediscovering them.

**Implementation steps:**
1. Replace the stub `hal_display_flush()` with a real ESP-IDF
   `driver/spi_master.h`-based implementation.
2. Read pins from `hal_display_config_t` (populated from
   `device_config_esp32c6.yaml`'s GPIO6/7/14/15/21/22) instead of the
   current hardcoded classic-ESP32 pin numbers.
3. Port the ST7789 init sequence (sleep-out, color mode, display-on) from
   `gfx_mono.h`'s proven-working Arduino_GFX usage.
4. Run the test pattern smoke test; iterate on rotation/offsets only if
   the ported values don't match (they should, since they're the same
   panel).

**Done when:** test pattern renders correctly oriented/colored on the
physical panel via the new HAL driver, driven from the kernel-skeleton
firmware built in Phase 2 (not from `main.cpp`).

**Push:** `feat(riscv): phase 3 - real ST7789 HAL driver for ESP32-C6`

---

## Phase 4 — wire `apps/app_framework.h` to the real HAL

**Goal:** `apps/stdapps/*` can render on real hardware without any
per-app changes — this is the layer that currently hardcodes
`ssd1306_model.c`.

**Test first (TDD):**
- Existing simulator tests for `app_framework.h` (check
  `docs/agent-guide.md` for their location) must keep passing unchanged —
  this phase must not alter sim behavior. Add a compile-time check (e.g.
  a build of the new `esp32-c6-kernel` env) proving the real-hardware
  branch compiles and does NOT pull in `ssd1306_model.c` for that target.

**Implementation steps:**
1. Add an `#if defined(ARDUBOT_TARGET_ESP32) ... #else ... #endif` branch
   in `apps/app_framework.h` for every `app_display_*` function, routing
   to `hal_display_*` (Phase 3's driver) instead of `ssd1306_model_*` when
   building for real hardware.
2. Confirm the sim build (`ARDUBOT_BUILD_SIM`) is completely unaffected —
   run the full existing sim test suite.
3. Rebuild the Phase 2 kernel-skeleton firmware with this change; confirm
   it still boots (display calls now route somewhere real, even if no app
   calls them yet).

**Done when:** sim tests still green; ESP32-C6 kernel build compiles with
the new branch and boots.

**Push:** `feat(riscv): phase 4 - app_framework real-hardware display routing`

---

## Phase 5 — bring up `apps/stdapps/*` on real hardware

**Goal:** the actual payoff — `launcher`, `counter`, `pomodoro`,
`taskmgr`, `pong` running for real, via the real kernel, on the physical
board.

**Test first (TDD):**
- For each app, the existing sim-side app tests (if any — check
  `apps/stdapps/<app>/` for test files) must stay green; this phase adds
  no new app logic, only a new target they also build for.
- Hardware smoke test per app (manual, documented, one checklist item
  each): flash with that app as the boot app (mirroring `sim_main.c`'s
  `app_start("pomodoro")` pattern, but for the new ESP32-C6 kernel env),
  confirm it renders and responds to the board's real buttons.

**Implementation steps:**
1. Add `apps/stdapps/*` to the `esp32-c6-kernel` PlatformIO env's build
   sources (mirroring `apps/stdapps/CMakeLists.txt`'s
   `ARDUBOT_ALL_STDAPPS` list, adapted for PlatformIO's `build_src_filter`
   syntax as already done for the `nodemcu`/`esp32-c6` envs).
2. Wire the board's real button GPIOs to `sim_gpio`/`SIM_KEY_*` input
   (reuse the pin mapping already in `boards/esp32-c6-lcd/src/main.cpp`'s
   `poll_buttons()` for reference — do not duplicate its logic, adapt it
   into the kernel/HAL input path if one doesn't already exist for real
   hardware).
3. Bring up one app at a time, in this order (simplest first):
   `counter` → `pomodoro` → `taskmgr` → `pong` → `launcher` (launcher last
   since it's the most complex UI and others validate the pipeline
   first).
4. For each app: flash, run its smoke test, fix anything that's
   simulator-specific and doesn't translate (expect: timing assumptions,
   anything reading `ssd1306_model` internals directly instead of through
   `app_framework.h`).

**Done when:** all five apps run correctly on the physical board via the
real kernel, with real button input, matching their simulator behavior as
closely as the hardware allows.

**Push:** `feat(riscv): phase 5 - apps/stdapps running on real ESP32-C6 hardware`

---

## Phase 6 — cutover decision (not a default outcome)

Once Phase 5 is proven solid (ran reliably across multiple
flash/power-cycle sessions), decide whether to:
(a) keep `main.cpp` as a permanent parallel fallback env, or
(b) retire it now that the kernel path is proven.

This decision should be made explicitly with the user at that time, not
assumed — do not delete or stop maintaining `main.cpp` as part of any
earlier phase.

---

## What stays as-is throughout phases 0-5

`boards/esp32-c6-lcd/src/main.cpp` (the Arduino sketch) and the existing
`[env:esp32-c6]` PlatformIO env are the only things that have ever
actually run on this physical board. Leave them alone and keep them
flashable throughout — every phase above adds a new, separate
`esp32-c6-kernel`-style env rather than modifying the existing one.
