# Plan: real-hardware ArdubotOS (kernel + apps/stdapps) on ESP32-C6 (RISC-V)

Status: **not started — scoping only.** This document exists so the decision
to attempt this is made deliberately, with the real blockers known up
front, rather than discovered mid-build.

## Why this doesn't work today

Right now, two completely separate things both call themselves ArdubotOS:

1. **The simulator** (`make run ARCH=sim`) — the real kernel
   (`kernel/`), the real app framework (`apps/`, `apps/stdapps/`), built
   via CMake, running on the host.
2. **The ESP32-C6 board's firmware** (`boards/esp32-c6-lcd/src/main.cpp`) —
   a standalone Arduino/PlatformIO sketch that reimplements the same ideas
   (launcher, counter, info, stopwatch, pong, task manager, pomodoro)
   independently, sharing no code with (1).

The physical device currently only runs (2). Getting it to run (1) instead
— "link apps and the OS, one file, no separate code" — is blocked on four
independent things, in dependency order:

### 1. `kernel/host_stack.c` has no RISC-V implementation (blocks everything)

This is the primitive that gives each cooperative task its own stack via
inline assembly, hand-written per CPU architecture. Today it has branches
for `_WIN32`, `__aarch64__`, `__x86_64__`, and `__xtensa__` (classic
ESP32/ESP8266) — **no `__riscv__`/`__riscv` branch**. The ESP32-C6 is a
32-bit RISC-V chip (not Xtensa). Compiling the kernel for it today hits:

```c
#error "host_call_on_stack: unsupported host architecture"
```

**What's needed:** a `#elif defined(__riscv)` branch implementing the same
contract (switch `sp`, call `fn(arg)`, never return) using RISC-V's calling
convention — `sp` is `x2`, the first argument register is `a0` (`x10`), and
an indirect call is `jalr`. Sketch (unverified, needs real testing on
hardware or QEMU):

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

This is the critical path — nothing else below matters until this exists
and is proven to actually context-switch correctly on real hardware (a
host-side RISC-V QEMU test, if one can be set up, would be far cheaper to
iterate on than physical flashing).

### 2. `hal/arch/esp32/hal_display_esp32.c` is a non-functional stub

- `hal_display_flush()` does nothing (`return 0`) — no code path has ever
  actually pushed a framebuffer to a real panel through this file.
- Pins are hardcoded to classic-ESP32 numbering (GPIO23/18 for
  MOSI/SCLK) — wrong for this board, which uses GPIO6/7/14/15/21/22 (see
  `device_config_esp32c6.yaml`).
- No ST7789 init sequence (sleep-out, color mode, display-on, etc.).

**What's needed:** a real driver, analogous to what
`boards/esp32-c6-lcd/src/gfx_mono.h` already does via Arduino_GFX, but
written against the ESP-IDF SPI driver (`driver/spi_master.h`) instead,
and reading pins from the existing `hal_display_config_t` rather than
hardcoding them.

### 3. No startup/linking path for this chip

- `cmake/toolchain.cmake`'s bare-metal fallback (when `IDF_PATH` isn't set)
  uses `boards/esp32-devkitc/esp32.ld` + `startup_esp32.S` — both written
  for classic Xtensa ESP32, wrong ISA for the C6.
- The alternative is pointing `IDF_PATH` at a real ESP-IDF install, which
  handles startup/linking itself (sidestepping new linker scripts) but
  requires actually installing the ESP-IDF SDK — a different thing from
  PlatformIO's bundled toolchain packages (which *did* download fine this
  session). Whether the ESP-IDF installer itself reaches the network
  successfully in a given environment is untested.
- A third option: a PlatformIO environment with `framework = espidf` (the
  board profile already lists `espidf` as a supported framework) — this
  would let PlatformIO manage the ESP-IDF toolchain/SDK as a package like
  it already does for Arduino, avoiding a manual `IDF_PATH` install. This
  is the most promising-looking option but is unverified for a project
  with ArdubotOS's existing CMake structure — PlatformIO's `espidf`
  framework expects an ESP-IDF *component* layout
  (`idf_component_register` etc.), which `kernel`/`apps`/`hal` don't use
  today.

### 4. `apps/app_framework.h` has no real-hardware display path

Every `app_display_*` call in this file routes unconditionally to
`ssd1306_model.c` (`sim/models/`) — a pure simulator construct with no
real hardware behind it. There's no `#if`/`#else` branch that would route
to `hal_display_*` instead when building for real ESP32 hardware. This
needs to exist before any `apps/stdapps/*` app could render anything on a
real screen, even after (1)-(3) are solved.

## Recommended order of attack, if this is picked up

1. Write the RISC-V `host_call_on_stack` branch. Validate it in isolation
   first — ideally via QEMU's RISC-V board support, or at minimum a
   from-scratch "does a context switch happen" test — before touching
   display code at all. This is small, self-contained, and testable
   without a working display.
2. Decide the toolchain question (real `IDF_PATH` install vs. PlatformIO
   `espidf` framework vs. a from-scratch RISC-V linker script/startup)
   before writing more code — this determines the whole build layout and
   shouldn't be discovered halfway through.
3. Write the real `hal_display_esp32.c` ST7789 driver for this board's
   pins, reusing the init sequence already proven to work in
   `boards/esp32-c6-lcd/src/gfx_mono.h`.
4. Wire `apps/app_framework.h`'s display calls to branch to
   `hal_display_esp32.c` under `ARDUBOT_TARGET_ESP32`.
5. Only then: build `apps/stdapps/*` for real hardware and see what
   actually renders.

Each step should be independently buildable/testable before moving to the
next — this is not a "do it all, then debug" task given how much of it is
unproven.

## What stays as-is either way

`boards/esp32-c6-lcd/src/main.cpp` (the Arduino sketch) is the only thing
that has ever actually run on this physical board, and should be left
alone and kept flashable throughout — if this plan gets picked up, treat
it as a separate, parallel effort, not a replacement, until the kernel
path has fully proven itself on real hardware.
