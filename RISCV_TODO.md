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

**Decision log:**
- Toolchain chosen: **PlatformIO's existing `framework = arduino`** for
  ESP32-C6 (the same one `[env:esp32-c6]`/`main.cpp` already use) — plain
  C kernel sources compiled as extra sources into that framework's own
  build, with a new minimal `setup()`/`loop()` entry point replacing
  `main.cpp`. **Not** PlatformIO's `espidf` framework, and **not** a
  from-scratch linker/startup script.
- Why: discovered while starting Phase 2 that `~/.platformio/packages/`
  already has `framework-arduinoespressif32` fully installed and proven
  (it's what flashes `main.cpp` today) — Arduino's own startup/linking
  already works on this exact board, so there's no new linker script or
  ESP-IDF component restructuring needed at all. This also surfaced a
  real RISC-V cross-GCC at
  `~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-gcc`
  (installed by pioarduino for this board, no `brew trust` needed),
  closing part of Phase 1's QEMU gap: the `__riscv` branch cross-compiles
  cleanly and its generated assembly was inspected by hand (`objdump -d`)
  to confirm it matches intent — `sp`/`a0` set correctly, `jalr` to the
  right register — though it's still not been *run* under emulation or
  on real silicon.

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
`tests/unit/test_host_stack.c`. While starting Phase 2, a real RISC-V
cross-GCC was found already installed at
`~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-gcc`
(pioarduino's own toolchain, no `brew trust` needed) and used to
cross-compile this branch and hand-inspect the generated assembly
(`objdump -d`) — it matches intent (`sp`/`a0` set correctly, `jalr` to
the right register). Still not run under QEMU or real silicon, though:
**Phase 2 ended up NOT exercising this branch** — ESP32-C6 real hardware
needed a different scheduler backend entirely (`scheduler_esp32.c`, real
FreeRTOS tasks) because of a hardware stack-guard conflict unrelated to
this assembly's correctness (see Phase 2 below). This branch remains
useful only for the sim/host build's own cross-platform story, and is
still unverified by actual execution on RISC-V.

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

**Status: done — booting and running real tasks on physical hardware.**

New PlatformIO env `[env:esp32-c6-kernel]` in `platformio.ini`, building
`boards/esp32-c6-lcd/src_kernel/kernel_boot.cpp` (new file — `main.cpp` is
untouched) against the real kernel. `kernel_boot.cpp`'s `setup()` calls
`scheduler_init()` + `task_create()` + `scheduler_start()`; `loop()` calls
`scheduler_tick()` + `timers_process()` each iteration. A demo task prints
a tick count over serial once a second.

Flashing the FIRST version of this (using `kernel/scheduler.c`/
`host_stack.c` — the cooperative setjmp/longjmp fiber scheduler, same as
sim) **crashed immediately** on real hardware:

```
Guru Meditation Error: Core 0 panic'ed (Stack protection fault).
Detected in task "loopTask" at 0x42000032   <- inside host_call_on_stack's own asm
```

Root cause: ESP32-C6 has a hardware "assist_debug" stack-pointer monitor
that panics the instant `sp` leaves the bounds FreeRTOS registered for
whichever real task is "currently running" (`loopTask`, in this case).
ArdubotOS's fiber scheduler intentionally jumps `sp` to independent
`malloc()`'d stacks per task — a model this hardware monitor was never
designed to tolerate. Three escalating attempts to patch around it are
preserved in git history for anyone revisiting this:
1. A one-time `esp_hw_stack_guard_monitor_stop()` in `scheduler_init()` —
   didn't hold; a background FreeRTOS context switch (Wi-Fi/BT/idle task)
   silently re-arms the guard with `loopTask`'s original bounds at an
   unpredictable later point.
2. Re-arming the guard with the correct bounds at every fiber switch
   point (`host_call_on_stack`'s call site, every `longjmp`-based resume
   in `context_switch()`/`switch_to_main()`), wrapped in a brief
   interrupt-disable — closed the first race but opened a second,
   narrower one: the gap between re-enabling interrupts and the actual
   `mv sp` instruction executing, which real UART/system interrupt load
   hit *deterministically*, every boot.
3. Closing that second race properly would mean bracketing
   interrupt-disable around every `setjmp`/`longjmp` resume point
   scattered through `scheduler.c` (task sleep, yield, `scheduler_step`,
   `scheduler_start`, `task_create`) — assessed as too error-prone (a
   missed site means a silent watchdog hang, worse than the loud panic
   it replaces) and abandoned.

**Actual fix (implemented):** `kernel/scheduler_esp32.c` — a parallel,
from-scratch implementation of the exact same `scheduler.h` API, backing
each ArdubotOS task with a **real FreeRTOS task** (`xTaskCreate`) instead
of the raw stack-switch trick. `kernel/scheduler.c`/`host_stack.c` are
untouched and still used by the sim/host build (all 45 existing tests
still pass); `platformio.ini`'s `esp32-c6-kernel` env now builds
`scheduler_esp32.c` instead. `task_create`/`task_sleep`/`task_yield`/
`task_suspend`/`task_resume`/`scheduler_lock`/`scheduler_unlock`/task
introspection are all implemented for real (the only app-facing calls
found via a repo-wide grep); tickless idle, deep sleep, and ISR-nesting
tracking are explicit no-op stubs (documented in the file header) since
no app uses them yet — implement for real only when one actually needs
to.

**Confirmed on physical hardware** (serial log):
```
[kernel_boot] ArdubotOS real kernel starting (Phase 2 skeleton)
[kernel_boot] demo_task tick=1
[kernel_boot] scheduler started - boot banner printed, entering loop()
[kernel_boot] demo_task tick=2
...
[kernel_boot] demo_task tick=9
```
Ticks arrive reliably once per second, no crashes, no watchdog resets.

**Implication for later phases:** Phase 1's RISC-V `host_call_on_stack`
branch is no longer on the path to booting apps on this board (the ESP32
target uses `scheduler_esp32.c`, which never calls it) — it remains
useful only for the sim/host build's own portability story (if a RISC-V
host ever runs the sim) and is left in place, unverified under QEMU, as
originally scoped in Phase 1. Phase 4/5 ("wire `apps/app_framework.h` to
real hardware" / "bring up `apps/stdapps/*`") should keep using the real
FreeRTOS tasks from this phase, not revisit the fiber model.

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

**Status: done — confirmed visually on the physical panel.**

Implemented as `hal/arch/esp32/hal_display_esp32_arduino.cpp` (new file)
rather than fixing the raw-ESP-IDF `hal_display_esp32.c` stub this
section originally planned around: Phase 0/2 already settled on reusing
PlatformIO's proven Arduino framework instead of a from-scratch
ESP-IDF/linker path, and Arduino_GFX (already a `lib_dep` of
`[env:esp32-c6]`, already proven correct for this exact panel by
`gfx_mono.h`/`main.cpp`) made hand-rolling raw SPI/ST7789 timing a second
time pure risk with no benefit. `hal_display_esp32.c` (the original stub)
is left in the tree untouched, undocumented as dead weight — it's a
reasonable starting point if a future from-scratch/espidf path is ever
actually taken, so it stays rather than being deleted.

Reused the exact rotation/offset values proven in `gfx_mono.h`
(`Arduino_ST7789(..., 1 /*rotation*/, true /*IPS*/, 172, 320, 34, 0, 34,
0)`) and this board's pins from `device_config_esp32c6.yaml`
(GPIO6/7/14/15/21/22). `kernel_boot.cpp` was extended with a
`draw_phase3_test_pattern()` call in `setup()` — red/green/blue vertical
stripes with a white border, drawn through `hal_display_*` directly (not
through `apps/app_framework.h` yet - that's Phase 4). Flashed, and the
user confirmed visually: the stripes and border render correctly on the
physical screen, correctly oriented, right after the "hal_display_init
ok" serial line and before the kernel scheduler's own boot banner.

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

**Status: done — a real stdapp (info) runs on physical hardware through
the full app framework, confirmed visually.**

Scope grew well beyond "wire the display calls": the full `apps` library
(`app.c`, `app_kit.c`, `app_ui.c`, `canvas.c`, `syscall.c`, `stdlog.c`,
`input.c`, `ardubot_keys.c`, `device_info.c`) plus `sim/sim_gpio.c`
(despite its name, a portable pin/key-state table with no simulator
dependency) and the real `hal_gpio_esp32.c` driver had never been
compiled for ESP32 before — getting there surfaced and fixed several
real, previously-latent bugs, none specific to this phase's original
plan:
- `hal/include/hal_power.h` defined its own mirror `esp_sleep_wakeup_cause_t`
  enum that collided with ESP-IDF's real `esp_sleep.h` the first time both
  were ever compiled together. Guarded behind `ARDUBOT_TARGET_ESP32`.
- `hal/arch/esp32/hal_gpio_esp32.c`'s ISR handler passed the wrong type to
  the callback (`gpio` instead of `gpio->pin`) and never called
  `gpio_install_isr_service()` before its first `gpio_isr_handler_add()`.
- `apps/stdlog.c` had a `Serial.begin()` call inside a plain `.c` file
  (self-labeled "pseudo-code" in its own comment) that could never have
  compiled - dead, unused, removed.
- `apps/app_framework.h`'s `app_button_init()` passed virtual pin-slot
  numbers (from `apps/app_kit.c`'s sequential `g_next_pin`, starting at
  20) straight into real `hal_gpio_open()`/`gpio_config()` - on this
  board, slots 21/22 are the ST7789's real RST/BL pins, so binding a few
  keys reconfigured the display driver's own control pins out from under
  it and hung/faulted. Fixed by skipping the (already functionally inert
  - it always passed a NULL callback) real-hardware GPIO path entirely
  on ESP32; the real key-dispatch mechanism is `sim_gpio_handle_key()`,
  driven by actual button polling in `kernel_boot.cpp`.
- `apps/app_framework.h`'s ESP32 display singleton (`g_esp32_display`)
  was a header-scope `static`, so every `.c` file including the header
  got its own independent copy and each one would separately
  `hal_display_open()`+`init()` the same real SPI/GFX driver - fixed with
  a true file-scope singleton inside `hal_display_esp32_arduino.cpp`
  itself, idempotent regardless of how many per-TU copies call it.
- `boards/esp32-c6-lcd/src_kernel/kernel_boot.cpp`'s `loop()` copied
  `sim_main.c`'s "`dt==0` → tick once anyway" fallback without accounting
  for the fact that Arduino's `loop()` (unlike `sim_main.c`'s SDL loop,
  paced to ~1ms/iteration by its own `sim_time_sleep_ms(1)`) has no
  pacing at all and runs far faster than 1ms/iteration - ticked the
  scheduler roughly once per `loop()` iteration instead of once per real
  millisecond, racing "uptime" far ahead of wall-clock time (~100x).
  Fixed by skipping the tick entirely when no real time has elapsed.
- `apps/app_ui.h`'s `content_x`/`content_w` (left/right margin) were
  hardcoded 0/full-width - invisible in the sim's SDL window, visibly
  flush against a real panel's bezel. Made configurable
  (`ARDUBOT_UI_PADDING`, `device_config_esp32c6.yaml`'s new `app_kit:`
  block) and applied to the title/help bar text too (the bar
  *backgrounds* intentionally still span full width).
- `apps/app_kit.c`'s default app frame rate was a hardcoded `30` -  made
  configurable (`ARDUBOT_DEFAULT_FPS`) the same way; an app's own
  manifest `.fps` always wins when set.
- `apps/stdapps/info/info_app.c` redrew (full `app_ui_begin_frame()`
  clear + redraw) every single call at its frame rate even though
  `app_is_dirty()` never actually gets cleared anywhere in this codebase
  (`app_clear_dirty()` exists, nothing calls it) - visible as a flash on
  a real panel with no double buffer. Fixed at the app level: the static
  parts (title bar, the two lines that never change) draw once in
  `on_init()`; `on_frame()` now only repaints the one line that actually
  changes (uptime seconds), and only when it actually changes.

None of these would have been caught without actually compiling and
running on real hardware - sim never exercised any of these code paths
for real GPIO/display/timing behavior.

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
