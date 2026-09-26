# ArdubotOS - Complete Implementation Plan

## Project Overview

**ArdubotOS** - A multi-architecture embedded OS for Arduino devices (AVR, ESP8266, ESP32) with:
- Preemptive/cooperative hybrid multitasking
- Plug-and-play driver framework with dynamic modules
- Flash + SD storage (LittleFS + FatFS)
- Full game engine (2D/3D, Lua/WASM, asset pipeline)
- UI framework (flex/grid, widgets, animations, themes)
- < 10µA deep sleep power management
- Arduino IDE library distribution
- SDL2 simulator for macOS/Linux/Windows development

---

## Architecture Decisions (Locked)

| Area | Decision |
|------|----------|
| **Target Hardware** | ESP8266 (min 32KB RAM), ESP32, Mega2560, Raspberry Pi Pico family (RP2040, RP2350), extensible to ARM Cortex-M |
| **Multitasking** | Hybrid: cooperative on AVR/ESP8266, preemptive on ESP32+ |
| **Storage** | Both: LittleFS (flash) + FatFS (SD) with unified VFS |
| **Games** | Full: scalable 2D/3D engine, Lua/WASM scripting, asset pipeline |
| **Development** | From scratch (C11), TDD mandatory |
| **App Sandbox** | Cooperative: same address space, trusted apps |
| **Arduino IDE** | Full OS as `#include <ArdubotOS.h>` with prebuilt libs |
| **Power Target** | < 10µA deep sleep (ESP32 RTC/ULP) |
| **Display** | SPI + I2C + 8/16-bit parallel + RGB (ESP32) |
| **UI Framework** | Full: containers, widgets, list/grid, text input, scroll, animations, themes |
| **Assets** | Build-time multi-resolution in `.arpak` format |
| **Simulator** | SDL2 + PortAudio, headless CI, macOS/Linux/Windows |

---

## Rules (All Enforced in CI)

1. **TDD Mandatory** - Test first (Red→Green→Refactor), pre-commit hook blocks untested code, 80% line / 70% branch coverage gate
2. **SOLID Principles** - HAL interfaces, dependency inversion, Liskov substitution via contract tests
3. **DRY** - Shared helpers in `common/`, cpd (copy-paste detector) in CI
4. **Emulator First** - Phase 0 complete before Phase 1
5. **Cross-Platform** - macOS (dev), Linux/Windows (CI)
6. **Power First** - Tickless idle, per-driver PM, deep sleep < 10µA
7. **App/Plugin Framework** - Cooperative tasks, syscall API, dynamic `.ardmod` modules
8. **Arduino IDE Library** - Single header, prebuilt static libs per board variant
9. **Multi-resolution Display** - Virtual canvas, flex/grid UI, build-time assets
10. **Documentation Kept Current** - Any change to a public API, CLI flag, or CMake option updates the matching doc in `docs/` (and `AGENTS.md`/`CLAUDE.md` for agent-facing workflow changes) in the same commit

---

## Repository Structure

```
ardubot/
├── CMakeLists.txt
├── Kconfig
├── PLAN.md
├── .github/workflows/
│   ├── sim.yml          # SDL2 simulator tests (Ubuntu, Windows, macOS)
│   └── code-quality.yml # clang-tidy, cpd, coverage
├── .pre-commit-config.yaml
├── cmake/
│   ├── toolchain.cmake
│   └── kconfig.cmake
├── kernel/
│   ├── scheduler.c/h
│   ├── ipc.c/h
│   ├── alloc.c/h
│   ├── time.c/h
│   └── power.c/h
├── hal/
│   ├── include/hal_*.h  # 8 interfaces
│   └── arch/
│       ├── esp32/
│       ├── esp8266/
│       ├── avr/
│       ├── rp2040/
│       └── sim/
├── fs/
│   ├── vfs.c/h
│   ├── littlefs/
│   └── fatfs/
├── drivers/
│   ├── display/
│   ├── sensor/
│   └── ...
├── modules/
│   ├── loader.c/h
│   └── format.ardmod
├── game/
│   ├── ecs.c/h
│   ├── renderer.c/h
│   ├── audio.c/h
│   ├── scripting/
│   └── assets/
├── apps/
│   ├── shell/
│   ├── filemgr/
│   ├── settings/
│   └── ota/
├── sim/
│   ├── sim_main.c
│   ├── sim_video.c
│   ├── sim_audio.c
│   ├── sim_storage.c
│   ├── sim_net.c
│   ├── sim_wifi.c
│   ├── sim_ble.c
│   ├── sim_gpio.c
│   ├── sim_i2c.c
│   ├── sim_spi.c
│   ├── sim_adc.c
│   ├── sim_pwm.c
│   ├── sim_rtc.c
│   └── models/
│       ├── ssd1306_model.c
│       ├── bmp280_model.c
│       └── ...
├── tools/
│   ├── ardubot-cli/
│   └── asset-pack/
├── tests/
│   ├── unity/              # Unity framework (submodule)
│   ├── unit/               # Fast host tests
│   ├── integration/        # Simulator tests
│   └── hardware/           # QEMU + physical
├── boards/
│   ├── nodemcu/
│   ├── esp32-devkitc/
│   ├── mega2560/
│   └── pico/
├── docs/
└── scripts/
    ├── tdd_check.py
    └── check_coverage.cmake
```

---

## 15-Month Phased Roadmap

### Phase 0: Emulator + TDD + CI (Weeks 1-2) ✅ **START HERE**

| Day | Task | Test First |
|-----|------|------------|
| 1 | Repo init: CMake dual-target, Kconfig, dirs | `test_cmake_config.c` |
| 1 | SDL2 window + event loop (software renderer) | `test_sim_video_init.c` |
| 2 | Build system: host + esp32 + esp8266 + avr + rp2040 targets | `test_build_targets.c` |
| 2 | Minimal kernel: task create, cooperative scheduler, time | `test_kernel_boot.c` |
| 3 | HAL stubs (sim): GPIO, I2C, SPI, UART, Display | `test_hal_gpio_sim.c` |
| 3 | Simulator main: args, HAL init, kernel start | `test_sim_args.c` |
| 4 | **Headless mode**: no window, runs test tasks, exits | `test_headless_exit.c` |
| 4 | JUnit XML output | `test_junit_output.c` |
| 5 | Flash/SD image files + LittleFS mount | `test_sim_storage.c` |
| 6 | Device model registry + I2C/SPI bus sim | `test_sim_device_reg.c` |
| 6 | **SSD1306 model**: renders to SDL2 texture | `test_ssd1306_model.c` |
| 7 | **BMP280 model**: I2C registers → temp/pressure | `test_bmp280_model.c` |
| 7 | Virtual GPIO: keyboard → pin mapping | `test_sim_gpio_keys.c` |
| 8 | PortAudio callback → `hal_audio_write()` stub | `test_sim_audio.c` |
| 8 | Pre-commit hook: `tdd_check.py` | `test_tdd_hook.c` |
| 9 | GitHub Actions: Ubuntu + Windows + macOS | `.github/workflows/sim.yml` |
| 9 | Coverage: lcov → HTML + Cobertura, gate 80/70 | `test_coverage_gate.c` |
| 10 | **Demo & Verify**: interactive + headless pass | Integration test |

**Success Criteria:**
```bash
./ardubot-sim --flash-image=flash.img --sd-image=sd.img  # Interactive works
./ardubot-sim --headless --test=all --junit=results.xml --coverage=cov.info  # CI passes
git commit -m "feat: new thing"  # FAILS if no test (TDD enforced)
# Windows + macOS + Ubuntu CI all green
```

---

### Phase 1: Kernel + HAL + Power (Months 1-3)

- Cooperative scheduler with priority, static task table
- Tickless idle, `os_idle()` → light sleep → deep sleep
- HAL: GPIO, I2C, SPI, UART, Display, Audio, Net, Storage
- Deep sleep ESP32/ESP8266: RTC timer + GPIO wake + ULP
- CPU frequency scaling (ESP32: 240→80→10MHz)
- Per-driver PM: `hal_*_suspend()` / `hal_*_resume()`

---

### Phase 2: Storage + VFS + OTA (Months 3-5)

- LittleFS on flash (wear-leveling, power-loss safe)
- FatFS on SD (SPI/SDIO, long filenames)
- Unified VFS: `open/read/write/seek/stat`, mount points `/flash`, `/sd`
- Config KV store in LittleFS
- A/B OTA with signature verification + rollback

---

### Phase 3: Driver Framework + Modules (Months 5-7)

- Driver model: `probe/remove/open/read/write/ioctl`, refcounting
- Core drivers built-in: GPIO, I2C bus, SPI bus, UART, WiFi, displays
- Dynamic module loader: `.ardmod` (stripped ELF + manifest + CRC)
- Device registry: `/dev/gpio0`, `/dev/i2c0`, `/dev/display0`
- Hotplug: I2C/SPI scan on demand, udev-style events

---

### Phase 4: App Framework + Syscalls + UI (Months 7-11)

- Syscall layer: capability tokens, function pointer table
- App lifecycle: install, start, stop, uninstall, resource limits
- Plugin API: drivers, codecs, protocols as `.ardmod`
- **Event System (Kernel-level):**
  - Pub/sub event bus: `os_event_subscribe(topic, callback)`, `os_event_publish(topic, data)`
  - Typed events: system (sleep/wake, battery, network), driver (hotplug, data ready), UI (input, focus, animation)
  - Per-app event namespaces: auto-cleanup on app stop
  - Event priorities + async delivery (work queue), sync option for critical paths
- **Input Framework:**
  - Virtual input devices: keyboard, keypad, 5-way nav, encoder, touch, buttons
  - Per-app input mapping: `input_map_t` registered on app start, swapped on app switch
  - Button handlers: `onTap`, `onLongTap`, `onDoubleTap`, `onHold`, `onRelease` with configurable thresholds
  - Key events: `onKeyDown`, `onKeyUp`, `onKeyRepeat` with key codes + modifiers
  - Gesture recognizers: tap, long press, drag, swipe, pinch (composable)
  - Focus system: widget focus chain, tab order, focus events
- **Full UI Framework:**
  - Containers: Flex (row/col), Grid, Scroll, Stack, Tab
  - Widgets: Label, Button, Image, Slider, Switch, Checkbox, List, GridView, TextInput, Progress, Spinner
  - Animation: Property animator (easing), transitions, keyframes
  - Theming: CSS-like (colors, fonts, spacing, border, radius, states)
  - Input: Touch, keyboard, encoder, 5-way nav, gestures
- Virtual canvas: 320x240 logical → physical via scaler

---

### StdApps: Standard Applications (Phase 4.5)

Pre-built applications included in the OS image:

| App | Description | Capabilities |
|-----|-------------|--------------|
| **counter** | Simple increment/decrement counter with UI | DISPLAY, EVENT |
| **settings** | System settings (WiFi, display, power) | DISPLAY, NET, WIFI, POWER, CONFIG |
| **fileman** | File manager for flash/SD | DISPLAY, FS, STORAGE |
| **shell** | REPL for debugging, app management | DISPLAY, EVENT, APP_MGMT |
| **demo** | Hardware test (GPIO, I2C, SPI, sensors) | GPIO, I2C, SPI, SENSOR |

Build as `.ardmod` modules in `apps/stdapps/`, linked into OS image or loadable at runtime.

---

### Phase 5: Game Engine (Months 9-12)

| Tier | Features |
|------|----------|
| T1 (32KB) | Tilemap + sprites, fixed-point, 1-2 layers, PCM audio |
| T2 (256KB) | + 3D software rasterizer, texture atlas |
| T3 (512KB+) | + WASM (wasm3), asset streaming, shaders |

- ECS scene graph (~2KB base)
- Abstract renderer backend: `DrawList` → platform blit/3D
- Soft mixer (4-8 channels), ADPCM/PCM, DMA on ESP32
- Lua 5.4 (T2+) / minilua (T1), sandboxed per-game
- Asset pipeline: `.arpak` with multi-resolution images/fonts/themes

---

### Phase 6: Arduino IDE Library (Months 11-14)

```
ArdubotOS/ (Arduino library)
├── library.properties
├── src/
│   ├── ArdubotOS.h          # Single include
│   ├── ArdubotOS.cpp        # os_init(), os_start(), task API
│   ├── hal/                 # Prebuilt per arch
│   └── modules/             # Built-in drivers
├── variants/
│   ├── esp32/               # board.txt, libardubot_esp32.a
│   ├── esp8266/
│   └── avr/
├── examples/
│   ├── BlinkTask/
│   ├── HelloDisplay/
│   ├── WiFiScan/
│   └── GameTemplate/
└── keywords.txt
```

---

### Phase 7: Polish + v1.0 (Months 14-15)

- CLI: `ardubot flash`, `ardubot monitor`, `ardubot create-game`
- Package manager: `ardubot install <game>`, dependency resolution
- Debugging: GDB stub, printf-over-RTT, crash dump to flash
- Docs: Doxygen + mkdocs, porting guide, game dev tutorial
- CI/CD: GitHub Actions build all boards, hardware-in-loop tests

---

## Simulator Architecture (Phase 0 Detail)

### Dual-Track Approach

**Track 1: SDL2 Native Simulator (Daily Dev)**
- Fast iteration, graphics, audio, debugging
- `cmake -DARDUBOT_BUILD_SIM=ON -DARDUBOT_SIM_BACKEND=sdl2`
- Virtual peripherals: GPIO, I2C, SPI, Display, Audio, Network, WiFi, BLE, ADC, PWM, RTC
- Headless: `--headless --test --junit --coverage`

**Track 2: QEMU (CI / Full-System)**
- Unmodified firmware, real bootloader
- Boards: ESP32 (upstream), ESP8266 (Espressif fork), Mega2560 (AVR target), RP2040 (upstream)
- Custom `ardubot` machine for OS-specific peripherals

### Shared HAL Pattern

```c
// hal_gpio.h - identical API for all targets
typedef struct hal_gpio hal_gpio_t;
hal_gpio_t* hal_gpio_open(const char* path, hal_gpio_mode_t mode);
void hal_gpio_write(hal_gpio_t* gpio, bool level);
bool hal_gpio_read(hal_gpio_t* gpio);
void hal_gpio_close(hal_gpio_t* gpio);

// hal_gpio_sim.c - simulator implementation
hal_gpio_t* hal_gpio_open(const char* path, hal_gpio_mode_t mode) {
    return sim_gpio_alloc(path, mode);
}

// hal_gpio_esp32.c - hardware implementation
hal_gpio_t* hal_gpio_open(const char* path, hal_gpio_mode_t mode) {
    return esp32_gpio_alloc(path, mode);
}
```

---

## Key Technical Specifications

### Kernel (Phase 1)
- **Scheduler**: Cooperative round-robin + priority, context switch < 20 cycles
- **Memory**: TLSF allocator, compile-time RAM budget per tier
- **Time**: 64-bit microsecond ticks, soft timers
- **IPC**: Message queues + events (no mutexes on T1)
- **Power**: Tickless idle, per-driver PM callbacks

### Display HAL (Phase 1)
```c
typedef struct {
    uint16_t width, height;
    uint8_t rotation;      // 0, 90, 180, 270
    uint8_t bpp;           // 1, 16, 24
    hal_display_interface_t interface;  // SPI, I2C, PARALLEL, RGB
} hal_display_config_t;
```

### UI Framework (Phase 4)
```c
// Flex container
ui_container_t* ui_flex_create(ui_flex_dir_t dir, ui_justify_t justify, ui_align_t align, uint16_t gap);

// Widgets
ui_widget_t* ui_button_create(const char* text, ui_callback_t cb);
ui_widget_t* ui_list_create(ui_list_item_provider_t provider);
ui_widget_t* ui_text_input_create(const char* placeholder);
```

### Asset Pipeline (Phase 5)
```
.arpak format:
/assets/
  images/
    icon@1x.png    (128x128)
    icon@2x.png    (256x256)
    icon@3x.png    (384x384)
  fonts/
    roboto_16.fnt
    roboto_24.fnt
  theme/
    default.css
    dark.css
```
Build tool: `ardubot-asset-pack` (Python, runs in CMake)

---

## CI/CD Pipeline

```yaml
# .github/workflows/sim.yml
jobs:
  sim-test-ubuntu:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: sudo apt-get install -y libsdl2-dev libportaudio2 lcov
      - run: cmake -B build -DARDUBOT_BUILD_SIM=ON && cmake --build build
      - run: ./build/ardubot-sim --headless --test=all --junit=results.xml --coverage=coverage.info

  sim-test-windows:
    runs-on: windows-latest
    env: { SDL_RENDER_DRIVER: software }
    steps:
      - uses: actions/checkout@v4
      - run: choco install sdl2 portaudio cmake mingw -y
      - run: cmake -B build -DARDUBOT_BUILD_SIM=ON && cmake --build build --config Release
      - run: ./build/Release/ardubot-sim.exe --headless --test=all --junit=results.xml

  sim-test-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - run: brew install sdl2 portaudio cmake lcov
      - run: cmake -B build -DARDUBOT_BUILD_SIM=ON && cmake --build build
      - run: ./build/ardubot-sim --headless --test=all --junit=results.xml
```

---

## Risk Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| RAM too tight on ESP8266 | High | Blocks T1 support | Compile-time budget, drop Lua on T1, WASM only T2+ |
| Simulator diverges from hardware | Medium | False confidence | Shared HAL source, QEMU CI validates real firmware |
| Scope creep | Very High | Timeline slip | Hard phase gates, no new features until deliverable works on 3 targets |
| Solo burnout | High | Project stalls | 2-week sprints, demo-driven, 1 week buffer/month |
| QEMU peripheral gaps | Medium | CI incomplete | Prioritize ESP32/ESP8266, custom `ardubot` machine |

---

## Next Steps (Implementation Mode)

1. **Initialize git repo** with proper `.gitignore`
2. **Create Phase 0 files** in this order:
   - `CMakeLists.txt`, `Kconfig`, `cmake/toolchain.cmake`, `cmake/kconfig.cmake`
   - `kernel/scheduler.c/h`, `kernel/time.c/h` (minimal)
   - `hal/include/hal_*.h` (8 interfaces)
   - `hal/arch/sim/hal_*_sim.c` (stubs)
   - `sim/sim_main.c`, `sim/sim_video.c`, `sim/sim_time.c`
   - `tests/unit/test_cmake_config.c`, `test_sim_video_init.c`, `test_kernel_boot.c` (FAILING first)
   - `.github/workflows/sim.yml`, `.pre-commit-config.yaml`, `scripts/tdd_check.py`
3. **Commit each logical group** with conventional commits
4. **Verify CI passes** on all three platforms
5. **Proceed to Phase 1**

---

## Conventional Commit Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

Types: `feat`, `fix`, `test`, `refactor`, `docs`, `chore`, `ci`, `build`

Examples:
- `test(kernel): add failing test for task creation`
- `feat(kernel): implement cooperative scheduler`
- `refactor(hal): separate GPIO interface from implementation`
- `ci(sim): add macOS CI job with software renderer`

---

## File List for Phase 0 (Complete)

### Core Config
- `CMakeLists.txt` ✓
- `Kconfig` ✓
- `cmake/toolchain.cmake`
- `cmake/kconfig.cmake`

### Kernel (Minimal)
- `kernel/scheduler.h/.c`
- `kernel/time.h/.c`
- `kernel/alloc.h/.c` (stub)

### HAL Interfaces
- `hal/include/hal_gpio.h`
- `hal/include/hal_i2c.h`
- `hal/include/hal_spi.h`
- `hal/include/hal_uart.h`
- `hal/include/hal_display.h`
- `hal/include/hal_audio.h`
- `hal/include/hal_net.h`
- `hal/include/hal_storage.h`

### HAL Simulator Implementation
- `hal/arch/sim/hal_gpio_sim.c`
- `hal/arch/sim/hal_i2c_sim.c`
- `hal/arch/sim/hal_spi_sim.c`
- `hal/arch/sim/hal_uart_sim.c`
- `hal/arch/sim/hal_display_sim.c`
- `hal/arch/sim/hal_audio_sim.c`
- `hal/arch/sim/hal_net_sim.c`
- `hal/arch/sim/hal_storage_sim.c`

### Simulator
- `sim/sim_main.c`
- `sim/sim_video.c`
- `sim/sim_audio.c`
- `sim/sim_storage.c`
- `sim/sim_time.c`
- `sim/sim_gpio.c`
- `sim/sim_i2c.c`
- `sim/sim_spi.c`
- `sim/sim_net.c`
- `sim/sim_wifi.c`
- `sim/sim_ble.c`
- `sim/sim_adc.c`
- `sim/sim_pwm.c`
- `sim/sim_rtc.c`
- `sim/models/ssd1306_model.c`
- `sim/models/bmp280_model.c`

### Tests (TDD - Written First)
- `tests/unity/` (submodule)
- `tests/unit/test_cmake_config.c`
- `tests/unit/test_build_targets.c`
- `tests/unit/test_kernel_boot.c`
- `tests/unit/test_hal_gpio_sim.c`
- `tests/unit/test_hal_i2c_sim.c`
- `tests/unit/test_hal_display_sim.c`
- `tests/unit/test_sim_storage.c`
- `tests/unit/test_sim_device_reg.c`
- `tests/unit/test_ssd1306_model.c`
- `tests/unit/test_bmp280_model.c`
- `tests/unit/test_sim_gpio_keys.c`
- `tests/unit/test_sim_audio.c`
- `tests/unit/test_sim_args.c`
- `tests/unit/test_headless_exit.c`
- `tests/unit/test_junit_output.c`
- `tests/unit/test_tdd_hook.c`
- `tests/unit/test_coverage_gate.c`
- `tests/integration/test_sim_boot.c`

### CI & Scripts
- `.github/workflows/sim.yml`
- `.github/workflows/code-quality.yml`
- `.pre-commit-config.yaml`
- `scripts/tdd_check.py`
- `scripts/check_coverage.cmake`

---

## Ready for Implementation Mode

When you restart in implementation mode, begin with:
1. `git init` + `.gitignore`
2. Create all Phase 0 files listed above
3. Write **failing tests first** for each component
4. Implement to make tests pass
5. Commit with conventional messages
6. Push and verify CI on all three platforms

The plan is complete and ready for execution.
