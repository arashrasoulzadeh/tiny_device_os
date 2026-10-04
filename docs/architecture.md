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

```mermaid
flowchart TB
    subgraph user["User-facing"]
        apps["apps/<br/>shell, fileman, settings, pong, ..."]
        game["game/<br/>ECS, renderer, audio, scripting"]
    end

    subgraph system["System services"]
        drivers["drivers/<br/>probe/open/read/write/ioctl<br/>device registry, hotplug"]
        modules["modules/<br/>.ardmod dynamic loader"]
        fs["fs/<br/>VFS -> LittleFS (/flash), FatFS (/sd)<br/>config store, OTA A/B"]
    end

    subgraph core["Kernel"]
        sched["kernel/scheduler.c<br/>cooperative, priority ready lists<br/>tickless idle"]
        alloc["kernel/alloc.c<br/>TLSF allocator"]
        power["kernel/power.c<br/>sleep modes, wake sources"]
    end

    subgraph hw["Hardware abstraction"]
        hal["hal/include/hal_*.h<br/>GPIO, I2C, SPI, UART, Display, Net, Storage"]
    end

    subgraph impls["Implementations (one compiled in per build)"]
        sim["sim/<br/>SDL2 + PortAudio + device models"]
        esp32["hal/arch/esp32/"]
        esp8266["hal/arch/esp8266/"]
        avr["hal/arch/avr/"]
    end

    apps --> drivers
    apps --> fs
    apps --> sched
    game --> sched
    drivers --> modules
    drivers --> hal
    fs --> hal
    sched --> alloc
    sched --> power
    power --> hal
    hal -.compiled against.-> sim
    hal -.compiled against.-> esp32
    hal -.compiled against.-> esp8266
    hal -.compiled against.-> avr
```

Everything above `hal/` is portable C that never includes a backend header
directly - swapping `sim` for `esp32` at build time is the only thing that
changes which arrows on the bottom row are "real".

## Boot and the cooperative scheduler

ArdubotOS uses `setjmp`/`longjmp` host fibers (not OS threads) to give each
task its own stack while staying strictly cooperative - a task only gives up
control at `task_yield()`, `task_sleep()`, or `task_suspend()`, never
preemptively.

```mermaid
sequenceDiagram
    participant Main as main() / sim_main loop
    participant Sched as scheduler
    participant Task as a task (e.g. an app)

    Main->>Sched: scheduler_init()
    Note over Sched: creates the idle task (TASK_PRIO_IDLE)<br/>and the main pseudo-task
    Main->>Sched: task_create("myapp", entry, ...)
    Note over Sched: allocates a private stack,<br/>bootstraps via host_call_on_stack + setjmp
    Main->>Sched: scheduler_start()
    Sched->>Sched: scheduler_step()
    Sched->>Task: longjmp into highest-priority ready task
    Task->>Task: runs until task_sleep() / task_yield() / return
    Task->>Sched: longjmp back (switch_to_main)
    loop every ~1ms of wall-clock time
        Main->>Sched: scheduler_tick()
        Note over Sched: wakes any task whose wake_time has arrived
        Main->>Sched: scheduler_step()
    end
```

Only one task's stack is ever "live" at a time - the sim main loop above
*is* the idle context, which is why `idle_task()` (the dedicated
`TASK_PRIO_IDLE` task created in `scheduler_init()`) never actually runs:
`scheduler_step()` and `task_yield()` both explicitly skip
`TASK_PRIO_IDLE` when picking the next task to switch to.

## Memory: the TLSF allocator

`kernel/alloc.c` implements a small TLSF (Two-Level Segregated Fit) pool
allocator - O(1) malloc/free with low fragmentation, suited to a fixed-size
heap on a microcontroller. `os_malloc`/`os_calloc`/`os_realloc`/`os_free`
wrap a single static 32KB pool (`g_heap_memory`); `tlsf_create`/`tlsf_malloc`/
etc. are the lower-level API usable against any caller-provided buffer
(see `tests/unit/test_kernel_alloc.c` for direct pool tests).

```mermaid
flowchart LR
    req["tlsf_malloc(pool, size)"] --> fl["bucket = fls(size)<br/>(free_list[32], size-class per power of two)"]
    fl --> scan["scan that bucket for a block >= size<br/>(exact bucket may hold smaller blocks)"]
    scan -->|found| split{"block much bigger<br/>than needed?"}
    split -->|yes| carve["block_split(): carve off the<br/>remainder, re-insert as free"]
    split -->|no| use["mark used, return pointer"]
    carve --> use
    scan -->|none in any bucket >= fl| fail["return NULL"]
```

Freeing a block runs `block_merge()` first, coalescing with a physically
adjacent free neighbor (tracked via each block's `prev_phys`/`next_phys`,
independent of the free-list buckets) before reinserting it - this is what
keeps long-running allocate/free churn from fragmenting the pool into
unusable slivers.

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
