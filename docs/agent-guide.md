# Agent guide

Read this before changing code. `AGENTS.md` is the rule list. `PLAN.md` is the
locked design and the multi-month roadmap — much of it is not implemented yet.
This file is the map of the code that exists today.

## What is real vs planned

Implemented and exercised by host tests:

- Cooperative scheduler (`kernel/scheduler.c`): up to 16 tasks, priority ready
  lists, `task_sleep`, host fibers via `kernel/host_stack.c`.
- Time and a bump allocator (`kernel/os_time.c`, `kernel/alloc.c`).
- HAL interfaces in `hal/include/hal_*.h` with a working **sim** backend in
  `hal/arch/sim/` plus SDL/device models in `sim/`.
- Board backends under `hal/arch/{esp32,esp8266,avr}/` exist as per-arch files;
  they are not the day-to-day development target.
- App runtime: manifests, **`app_kit`** (`APP_DEFINE`, dirty redraw, key bind),
  plus lower-level helpers (`apps/app_kit.h`, `apps/app_framework.h`, `apps/app.c`).
  Authoring guide: `docs/apps.md`.
- Built-in apps: `apps/stdapps/counter_app.c` (kit example), `apps/stdapps/launcher_app.c`.
- VFS, LittleFS/FatFS glue, config store, OTA stubs under `fs/`.
- Driver ops (`probe`/`open`/`read`/`write`/`ioctl`) in `drivers/driver.h`.
- Simulator CLI: headless mode, `--test=all`, JUnit, coverage
  (`sim/sim_main.c`, `sim/sim_args.c`).

Still roadmap (do not assume these work, and do not invent them while fixing
something else): game engine, Lua/WASM, dynamic `.ardmod` loading as a product,
Arduino IDE packaging, deep-sleep power targets, `tests/hardware/`.

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

Use **`app_kit.h`** / `APP_DEFINE` — see [`docs/apps.md`](apps.md) and
`apps/stdapps/counter_app.c`.

1. Implement `on_init` / `on_frame` (and optional `on_cleanup`).
2. Bind keys with `app_bind_key`, redraw with `app_mark_dirty` + `app_text` /
   `app_textf` / `app_flush`.
3. End the file with
   `APP_DEFINE(my_app, "my_app", .version = "...", .on_init = ..., .on_frame = ...)`.
4. Add the `.c` file to `apps/stdapps/CMakeLists.txt`.
5. Install/start via `app_install_manifest(my_app_manifest, "my_app")` and
   `app_start("my_app")` (see `sim/sim_main.c`).

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
