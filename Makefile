# ArdubotOS Makefile wrapper
# Usage:
#   make                    # Build default (sim, Debug)
#   make ARCH=esp32         # Build for ESP32
#   make ARCH=esp8266       # Build for ESP8266
#   make ARCH=avr           # Build for AVR Mega2560
#   make ARCH=rp2040        # Build for RP2040
#   make BUILD=Release      # Release build
#   make run                # Run simulator (interactive)
#   make test               # Run headless tests
#   make clean              # Clean build directory
#   make config             # Show current configuration
#   make run                # Run the SDL2 emulator
#   make usb                # Ask target, read device_config.yaml, build + flash USB
#   make usb DEVICE=nodemcu # Non-interactive USB flash for NodeMCU
#   make monitor            # Open a serial monitor on the configured device
#   make create-app NAME=x  # Scaffold a new stdapp at apps/stdapps/x
#   make install-app SRC=.. # Install a package.json app from a local dir
#   make list-apps          # List installed stdapps and their metadata
#   make docs                # Build Doxygen API reference + mkdocs site
#
# Config file: build.mk (optional, auto-loaded if exists)
# Device pins/port: device_config.yaml (required for make usb)
# Wi-Fi ssid/password: device_secrets.yaml (gitignored; required for make usb)
# Environment variables also work: ARCH, BUILD, FLAGS, OUTPUT, JOBS, DEVICE, PORT

# ============================================================================
# Default configuration
# ============================================================================
ARCH       ?= sim
BUILD      ?= Debug
JOBS       ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
OUTPUT     ?= build/$(ARCH)/$(BUILD)
FLAGS      ?=
CMAKE_OPTS ?=
TEST_FILTER ?= all
JUNIT_FILE  ?= results.xml
COVERAGE_FILE ?= coverage.info
DEVICE_CONFIG ?= device_config.yaml
DEVICE      ?=
PORT        ?=

# Load user config if present
-include build.mk

# ============================================================================
# Architecture-specific CMake options
# ============================================================================
ifeq ($(ARCH),sim)
  CMAKE_ARCH_OPTS = -DARDUBOT_BUILD_SIM=ON
  TARGET_BINARY   = $(OUTPUT)/sim/ardubot-sim
  RUN_CMD         = $(TARGET_BINARY) --flash-image=flash.img --sd-image=sd.img
  TEST_CMD        = $(TARGET_BINARY) --headless --test=$(TEST_FILTER) --junit=$(JUNIT_FILE) --coverage=$(COVERAGE_FILE)
else ifeq ($(ARCH),esp32)
  CMAKE_ARCH_OPTS = -DARDUBOT_BUILD_ESP32=ON -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE)
  TARGET_BINARY   = $(OUTPUT)/ardubot.elf
  RUN_CMD         = echo "Flash with: esptool.py --chip esp32 write_flash 0x10000 $(TARGET_BINARY)"
else ifeq ($(ARCH),esp8266)
  CMAKE_ARCH_OPTS = -DARDUBOT_BUILD_ESP8266=ON -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE)
  TARGET_BINARY   = $(OUTPUT)/ardubot.elf
  RUN_CMD         = echo "Flash with: esptool.py --chip esp8266 write_flash 0x00000 $(TARGET_BINARY)"
else ifeq ($(ARCH),avr)
  CMAKE_ARCH_OPTS = -DARDUBOT_BUILD_AVR=ON -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE)
  TARGET_BINARY   = $(OUTPUT)/ardubot.elf
  RUN_CMD         = echo "Flash with: avrdude -p atmega2560 -c wiring -U flash:w:$(TARGET_BINARY):i"
else ifeq ($(ARCH),rp2040)
  CMAKE_ARCH_OPTS = -DARDUBOT_BUILD_RP2040=ON -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE)
  TARGET_BINARY   = $(OUTPUT)/ardubot.uf2
  RUN_CMD         = echo "Drag $(TARGET_BINARY) to RPI-RP2 drive"
else
  $(error Unknown ARCH: $(ARCH). Valid: sim, esp32, esp8266, avr, rp2040)
endif

# For real hardware (not sim), compile in only the stdapps that fit this
# device's profile - explicit device_config.yaml `apps:` list, or an
# auto-fit against each app's package.json "min_display" otherwise. Sim
# always gets everything, since it's the dev-iteration target.
ifneq ($(ARCH),sim)
  ifneq (,$(wildcard $(DEVICE_CONFIG)))
    ENABLED_APPS := $(shell python3 scripts/device_config.py --config $(DEVICE_CONFIG) apps --cmake-list 2>/dev/null)
    ifneq ($(ENABLED_APPS),)
      CMAKE_ARCH_OPTS += -DARDUBOT_ENABLED_APPS="$(ENABLED_APPS)"
    endif
  endif
endif

# ============================================================================
# Build directory setup
# ============================================================================
BUILD_DIR = $(OUTPUT)
CMAKE_CACHE = $(BUILD_DIR)/CMakeCache.txt

# ============================================================================
# Main targets
# ============================================================================
.PHONY: all configure build run test clean clean-all config help compile compile-clean compile-info usb usb-ports device-config monitor create-app install-app list-apps docs docs-doxygen docs-site coverage

all: build

# Configure CMake
configure: $(CMAKE_CACHE)

$(CMAKE_CACHE):
	@mkdir -p $(BUILD_DIR)
	cmake -B $(BUILD_DIR) \
	  -DCMAKE_BUILD_TYPE=$(BUILD) \
	  $(CMAKE_ARCH_OPTS) \
	  $(CMAKE_OPTS) \
	  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	  -DCMAKE_C_FLAGS="$(FLAGS)" \
	  -DCMAKE_CXX_FLAGS="$(FLAGS)"

# Build the project
build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

# Run the simulator (interactive)
run: build
	@echo "Running $(ARCH) simulator..."
	@$(RUN_CMD)

# Run headless tests
test: build
	@echo "Running tests for $(ARCH)..."
	@$(TEST_CMD)

# Clean build directory
clean:
	rm -rf $(BUILD_DIR)

# Clean all build directories
clean-all:
	rm -rf build/

# make for-<arch>  (or  make for <arch>): export the single-file,
# stripped ArdubotOS amalgamation for that arch (per the CMakeLists.txt
# source lists) into build-<arch>/ - ardubot_<arch>.h, ardubot_<arch>.c,
# and a README.md. No CMakeCache, no separate "raw" copy, no binaries.
# Also runs a best-effort cc -c test-compile and records the result in
# the README - see scripts/export_arch.py.
.PHONY: for-%
for-%:
	python3 scripts/export_arch.py --arch $* --out build-$* --device-config $(DEVICE_CONFIG)

# `make for <arch>` form: treat the word after "for" as the arch, not a
# separate make target, by swallowing it into a no-op rule.
ifeq (for,$(firstword $(MAKECMDGOALS)))
  FOR_ARCH := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(FOR_ARCH):;@:)
endif

.PHONY: for
for:
	python3 scripts/export_arch.py --arch $(FOR_ARCH) --out build-$(FOR_ARCH) --device-config $(DEVICE_CONFIG)

# Show current configuration
config:
	@echo "=== ArdubotOS Build Configuration ==="
	@echo "ARCH        : $(ARCH)"
	@echo "BUILD       : $(BUILD)"
	@echo "JOBS        : $(JOBS)"
	@echo "OUTPUT      : $(OUTPUT)"
	@echo "FLAGS       : $(FLAGS)"
	@echo "CMAKE_OPTS  : $(CMAKE_OPTS)"
	@echo "TEST_FILTER : $(TEST_FILTER)"
	@echo "TARGET      : $(TARGET_BINARY)"
	@echo "BUILD_DIR   : $(BUILD_DIR)"

# Quick rebuild (skip configure)
rebuild:
	cmake --build $(BUILD_DIR) -j$(JOBS) --target clean
	cmake --build $(BUILD_DIR) -j$(JOBS)

# Install (for hardware targets)
install: build
ifeq ($(ARCH),esp32)
	esptool.py --chip esp32 write_flash 0x10000 $(TARGET_BINARY)
else ifeq ($(ARCH),esp8266)
	esptool.py --chip esp8266 write_flash 0x00000 $(TARGET_BINARY)
else ifeq ($(ARCH),avr)
	avrdude -p atmega2560 -c wiring -U flash:w:$(TARGET_BINARY):i
else ifeq ($(ARCH),rp2040)
	@echo "Copy $(TARGET_BINARY) to RPI-RP2 drive"
else
	@echo "Install not supported for $(ARCH)"
endif

# ---------------------------------------------------------------------------
# USB: interactive target select + device_config.yaml → build + flash
# ---------------------------------------------------------------------------
# Reads port / LCD / buttons from device_config.yaml.
# Prompts "what to compile on" unless DEVICE= is set.
usb:
	@python3 scripts/usb_flash.py --config $(DEVICE_CONFIG) \
	  $(if $(DEVICE),--device $(DEVICE),) \
	  $(if $(PORT),--port $(PORT),) \
	  $(if $(JOBS),--jobs $(JOBS),)

usb-ports:
	@python3 scripts/device_config.py --config $(DEVICE_CONFIG) ports

device-config:
	@python3 scripts/device_config.py --config $(DEVICE_CONFIG) show
	@python3 scripts/device_secrets.py check
	@python3 scripts/device_config.py --config $(DEVICE_CONFIG) gen-header \
	  -o build/generated/device_config.h
	@python3 scripts/device_secrets.py gen-header \
	  -o build/generated/device_secrets.h

# Serial monitor on the configured (or DEVICE=/PORT=) target
monitor:
	@python3 scripts/ardubot.py monitor --config $(DEVICE_CONFIG) \
	  $(if $(PORT),--port $(PORT),)

# Scaffold a new stdapp: make create-app NAME=mygame
create-app:
	@test -n "$(NAME)" || (echo "usage: make create-app NAME=<app_name>"; exit 1)
	@python3 scripts/ardubot.py create-app $(NAME)

# Install a package.json-described app: make install-app SRC=../mygame
install-app:
	@test -n "$(SRC)" || (echo "usage: make install-app SRC=<path to package dir>"; exit 1)
	@python3 scripts/ardubot.py install $(SRC) $(if $(FORCE),--force,)

list-apps:
	@python3 scripts/ardubot.py list

# Doxygen (API reference) + mkdocs (narrative docs), both gitignored outputs
docs-doxygen:
	@command -v doxygen >/dev/null || (echo "doxygen not found - brew install doxygen"; exit 1)
	doxygen Doxyfile
	@echo "API docs: docs/doxygen/html/index.html"

docs-site:
	@command -v mkdocs >/dev/null || (echo "mkdocs not found - pip install -r docs/requirements.txt"; exit 1)
	mkdocs build
	@echo "Docs site: site/index.html"

docs: docs-doxygen docs-site

# Real line/branch coverage from the host test suite (separate build dir,
# since instrumented objects shouldn't mix with normal ones). Prints lcov's
# own summary - see docs/agent-guide.md for what this actually measures
# and why it isn't, and may never realistically be, 100%.
COVERAGE_DIR = build/coverage
coverage:
	@command -v lcov >/dev/null || (echo "lcov not found - brew install lcov / apt install lcov"; exit 1)
	cmake -B $(COVERAGE_DIR) -DARDUBOT_BUILD_SIM=ON -DARDUBOT_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(COVERAGE_DIR) -j$(JOBS)
	ctest --test-dir $(COVERAGE_DIR) -j$(JOBS) || true
	lcov --capture --directory $(COVERAGE_DIR) --output-file $(COVERAGE_DIR)/coverage.info \
	  --ignore-errors inconsistent,unused 2>/dev/null
	lcov --remove $(COVERAGE_DIR)/coverage.info \
	  '*/tests/*' '*/third_party/*' '*/build/*' '/usr/*' '/Applications/*' \
	  --output-file $(COVERAGE_DIR)/coverage.filtered.info \
	  --ignore-errors unused 2>/dev/null
	genhtml $(COVERAGE_DIR)/coverage.filtered.info --output-directory $(COVERAGE_DIR)/html -q
	@lcov --summary $(COVERAGE_DIR)/coverage.filtered.info
	@echo "HTML report: $(COVERAGE_DIR)/html/index.html"

# One compile_commands.json for clangd / Cursor: simulator commands win for
# shared sources, then device-only translation units from PlatformIO.
# IDE_PIO_ENVS overrides the device list; IDE_NO_PIO=1 skips device envs.
IDE_PIO_ENVS ?= esp32-c6 esp32-c6-kernel nodemcu
compile-commands: configure
	python3 scripts/ide_compile_db.py collect --sim $(BUILD_DIR)/compile_commands.json \
	  $(if $(IDE_NO_PIO),--no-pio,$(foreach env,$(IDE_PIO_ENVS),--env $(env)))

# ============================================================================
# Architecture-specific stripped/minified build
# Creates build/compiled/arch_name with only needed classes, stripped/uglified
# ============================================================================

# Architecture-specific source filters and compiler flags
ifeq ($(ARCH),sim)
  COMPILE_FILTER = -DARDUBOT_BUILD_SIM=1
  COMPILE_STRIP  = -g0 -Os
  COMPILE_DEFINES = -DARDUBOT_SIM=1 -DARDUBOT_ARCH_SIM=1
  LDFLAGS = $(shell pkg-config --libs sdl2 2>/dev/null || echo "-L/opt/homebrew/lib -lSDL2")
else ifeq ($(ARCH),esp32)
  COMPILE_FILTER = -DARDUBOT_BUILD_ESP32=1
  COMPILE_STRIP  = -g0 -Os -fdata-sections -ffunction-sections
  COMPILE_DEFINES = -DARDUBOT_ESP32=1 -DARDUBOT_ARCH_ESP32=1 -DCONFIG_FREERTOS_UNICORE=0
  LDFLAGS = -Wl,--gc-sections
else ifeq ($(ARCH),esp8266)
  COMPILE_FILTER = -DARDUBOT_BUILD_ESP8266=1
  COMPILE_STRIP  = -g0 -Os -fdata-sections -ffunction-sections -mlongcalls
  COMPILE_DEFINES = -DARDUBOT_ESP8266=1 -DARDUBOT_ARCH_ESP8266=1
  LDFLAGS = -Wl,--gc-sections
else ifeq ($(ARCH),avr)
  COMPILE_FILTER = -DARDUBOT_BUILD_AVR=1
  COMPILE_STRIP  = -g0 -Os -fdata-sections -ffunction-sections -mmcu=atmega2560
  COMPILE_DEFINES = -DARDUBOT_AVR=1 -DARDUBOT_ARCH_AVR=1 -DF_CPU=16000000UL
  LDFLAGS = -Wl,--gc-sections
else ifeq ($(ARCH),rp2040)
  COMPILE_FILTER = -DARDUBOT_BUILD_RP2040=1
  COMPILE_STRIP  = -g0 -Os -fdata-sections -ffunction-sections
  COMPILE_DEFINES = -DARDUBOT_RP2040=1 -DARDUBOT_ARCH_RP2040=1
  LDFLAGS = -Wl,--gc-sections
endif

# Compiled output directory (separated by arch)
COMPILED_DIR = build/compiled/$(ARCH)

# Source files grouped by subsystem (only compile what's needed per arch)
KERNEL_SRCS = kernel/scheduler.c kernel/os_time.c kernel/alloc.c kernel/event.c kernel/host_stack.c
HAL_SRCS    = hal/arch/$(ARCH)/hal_gpio_$(ARCH).c hal/arch/$(ARCH)/hal_i2c_$(ARCH).c \
              hal/arch/$(ARCH)/hal_spi_$(ARCH).c hal/arch/$(ARCH)/hal_uart_$(ARCH).c \
              hal/arch/$(ARCH)/hal_display_$(ARCH).c hal/arch/$(ARCH)/hal_audio_$(ARCH).c \
              hal/arch/$(ARCH)/hal_net_$(ARCH).c hal/arch/$(ARCH)/hal_storage_$(ARCH).c \
              hal/arch/$(ARCH)/hal_power_$(ARCH).c hal/arch/$(ARCH)/hal_ble_$(ARCH).c \
              hal/arch/$(ARCH)/hal_adc_$(ARCH).c hal/arch/$(ARCH)/hal_pwm_$(ARCH).c \
              hal/arch/$(ARCH)/hal_wifi_$(ARCH).c
FS_SRCS     = fs/vfs.c fs/littlefs/littlefs_vfs.c third_party/littlefs/lfs.c third_party/littlefs/lfs_util.c fs/fatfs/fatfs_vfs.c fs/config_store.c
DRIVER_SRCS = drivers/driver.c drivers/device_registry.c drivers/display_driver.c \
              drivers/gpio_driver.c drivers/i2c_driver.c drivers/spi_driver.c \
              drivers/uart_driver.c drivers/wifi_driver.c drivers/module.c
APP_SRCS    = apps/app.c apps/app_kit.c apps/app_utils.c apps/input.c apps/stdlog.c apps/syscall.c apps/ui.c apps/device_info.c \
              apps/stdapps/counter/counter_app.c apps/stdapps/counter/counter_icon.c \
              apps/stdapps/launcher/launcher_app.c \
              apps/stdapps/info/info_app.c apps/stdapps/info/info_icon.c \
              apps/stdapps/stopwatch/stopwatch_app.c apps/stdapps/stopwatch/stopwatch_icon.c \
              apps/stdapps/pong/pong_app.c apps/stdapps/pong/pong.c apps/stdapps/pong/pong_icon.c \
              apps/stdapps/settings/settings_app.c apps/stdapps/settings/settings_icon.c \
              apps/stdapps/fileman/fileman_app.c apps/stdapps/fileman/fileman_icon.c \
              apps/stdapps/shell/shell_app.c apps/stdapps/shell/shell_icon.c \
              apps/stdapps/demo/demo_app.c apps/stdapps/demo/demo_icon.c \
              apps/ui/components/canvas.c apps/ui/components/screen.c \
              apps/ui/components/menu.c apps/ui/components/catalog.c \
              apps/ui/components/icons.c apps/ui/components/status.c
SIM_SRCS    = sim/sim_gpio.c sim/sim_i2c.c sim/sim_spi.c sim/sim_storage.c \
              sim/sim_time.c sim/sim_video.c sim/sim_audio.c sim/sim_args.c \
              sim/models/ssd1306_model.c sim/models/bmp280_model.c

# Architecture-specific source selection
ifeq ($(ARCH),sim)
  ARCH_SRCS = $(KERNEL_SRCS) $(HAL_SRCS) $(FS_SRCS) $(DRIVER_SRCS) $(APP_SRCS) \
              $(SIM_SRCS) sim/sim_main.c sim/sim_net.c sim/sim_wifi.c sim/sim_ble.c \
              sim/sim_adc.c sim/sim_pwm.c sim/sim_rtc.c
  # Deduplicate (SIM_SRCS already lists several sim_*.c files)
  ARCH_SRCS := $(sort $(ARCH_SRCS))
else
  # For hardware targets, exclude simulator code
  ARCH_SRCS = $(KERNEL_SRCS) $(HAL_SRCS) $(FS_SRCS) $(DRIVER_SRCS) $(APP_SRCS)
endif

# Compiled object directory
COMPILED_OBJ_DIR = $(COMPILED_DIR)/obj
COMPILED_BIN_DIR = $(COMPILED_DIR)/bin
COMPILED_LIB_DIR = $(COMPILED_DIR)/lib

# Compiler for each architecture
ifeq ($(ARCH),esp32)
  CC = xtensa-esp32-elf-gcc
  AR = xtensa-esp32-elf-ar
  STRIP = xtensa-esp32-elf-strip
  OBJCOPY = xtensa-esp32-elf-objcopy
  CFLAGS_BASE = -std=c11 -Wall -Wextra -Wpedantic -Werror -mlongcalls -Wno-error=unused-parameter
else ifeq ($(ARCH),esp8266)
  CC = xtensa-lx106-elf-gcc
  AR = xtensa-lx106-elf-ar
  STRIP = xtensa-lx106-elf-strip
  OBJCOPY = xtensa-lx106-elf-objcopy
  CFLAGS_BASE = -std=c11 -Wall -Wextra -Wpedantic -Werror -mlongcalls -Wno-error=unused-parameter
else ifeq ($(ARCH),avr)
  CC = avr-gcc
  AR = avr-ar
  STRIP = avr-strip
  OBJCOPY = avr-objcopy
  CFLAGS_BASE = -std=c11 -Wall -Wextra -Wpedantic -Werror -mmcu=atmega2560 -DF_CPU=16000000UL -Wno-error=unused-parameter
else ifeq ($(ARCH),rp2040)
  CC = arm-none-eabi-gcc
  AR = arm-none-eabi-ar
  STRIP = arm-none-eabi-strip
  OBJCOPY = arm-none-eabi-objcopy
  CFLAGS_BASE = -std=c11 -Wall -Wextra -Wpedantic -Werror -mcpu=cortex-m0plus -mthumb -Wno-error=unused-parameter
else
  CC = gcc
  AR = ar
  STRIP = strip
  OBJCOPY = objcopy
  CFLAGS_BASE = -std=c11 -Wall -Wextra -Wpedantic -Werror -Wno-error=unused-parameter
endif

# Final compile flags per architecture
CFLAGS = $(CFLAGS_BASE) $(COMPILE_STRIP) $(COMPILE_FILTER) $(COMPILE_DEFINES) \
         -I. -Ikernel -Ihal/include -Ifs -Ifs/littlefs -Ifs/fatfs -Idrivers -Iapps -Iapps/ui/components -Isim -Isim/models -Ithird_party/littlefs

ifeq ($(ARCH),sim)
  CFLAGS += $(shell pkg-config --cflags sdl2 2>/dev/null || echo "-I/opt/homebrew/include -I/opt/homebrew/include/SDL2")
endif

# Compiled static library
COMPILED_LIB = $(COMPILED_LIB_DIR)/libardubot_$(ARCH).a

# ============================================================================
# Compiled build targets
# ============================================================================

.PHONY: compile compile-clean compile-info

# Main compile target - creates build/compiled/arch_name with stripped/uglified output
compile: $(COMPILED_LIB) $(COMPILED_BIN_DIR)/ardubot_$(ARCH).bin

# Compiled static library - keep directory structure in object names
COMPILED_OBJS = $(addprefix $(COMPILED_OBJ_DIR)/,$(ARCH_SRCS:.c=.o))

$(COMPILED_LIB): $(COMPILED_OBJS)
	@mkdir -p $(COMPILED_LIB_DIR)
	$(AR) rcs $@ $^
	@echo "Created static library: $@"

# Compile individual source files to object files
# Use explicit pattern substitution to handle directory structure
define COMPILE_RULE
$(COMPILED_OBJ_DIR)/$(1).o: $(1).c
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -Wno-error=incompatible-pointer-types -Wno-error=unused-variable -Wno-error=unused-parameter -Wno-error=unused-function -Wno-error=int-to-void-pointer-cast -Wno-error=gnu-zero-variadic-macro-arguments -c $$< -o $$@
endef

$(foreach src,$(ARCH_SRCS),$(eval $(call COMPILE_RULE,$(basename $(src)))))

# Final binary (stripped/uglified)
$(COMPILED_BIN_DIR)/ardubot_$(ARCH).bin: $(COMPILED_LIB)
	@mkdir -p $(COMPILED_BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(COMPILED_BIN_DIR)/ardubot_$(ARCH).elf $(COMPILED_LIB)
	$(STRIP) --strip-all --remove-section=.comment --remove-section=.note $(COMPILED_BIN_DIR)/ardubot_$(ARCH).elf 2>/dev/null || true
	# objcopy -O binary only works for ELF, not Mach-O; use ELF directly on macOS
	@if [ "$(ARCH)" = "sim" ]; then \
		cp $(COMPILED_BIN_DIR)/ardubot_$(ARCH).elf $(COMPILED_BIN_DIR)/ardubot_$(ARCH).bin; \
	else \
		$(OBJCOPY) -O binary $(COMPILED_BIN_DIR)/ardubot_$(ARCH).elf $(COMPILED_BIN_DIR)/ardubot_$(ARCH).bin 2>/dev/null || true; \
	fi
	@echo "Created stripped binary: $(COMPILED_BIN_DIR)/ardubot_$(ARCH).bin"
	@ls -la $(COMPILED_BIN_DIR)/ardubot_$(ARCH).bin 2>/dev/null || true

# Clean compiled output
compile-clean:
	rm -rf $(COMPILED_DIR)

# Show compile configuration
compile-info:
	@echo "=== Compile Configuration for $(ARCH) ==="
	@echo "CC          : $(CC)"
	@echo "CFLAGS      : $(CFLAGS)"
	@echo "COMPILED_DIR: $(COMPILED_DIR)"
	@echo "SOURCES     : $(words $(ARCH_SRCS)) files"
	@echo "LIB         : $(COMPILED_LIB)"
	@echo "BINARY      : $(COMPILED_BIN_DIR)/ardubot_$(ARCH).bin"

# Help
help:
	@echo "ArdubotOS Makefile wrapper"
	@echo ""
	@echo "Usage:"
	@echo "  make [ARCH=sim|esp32|esp8266|avr|rp2040] [BUILD=Debug|Release] [FLAGS=...] [JOBS=N]"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build project (default)"
	@echo "  configure     - Run CMake configure"
	@echo "  build         - Build project"
	@echo "  run           - Run simulator (interactive, ARCH=sim only)"
	@echo "  test          - Run headless tests with JUnit/coverage"
	@echo "  clean         - Clean current build directory"
	@echo "  clean-all     - Clean all build directories"
	@echo "  config        - Show current configuration"
	@echo "  rebuild       - Clean and rebuild"
	@echo "  install       - Flash to hardware (requires tool)"
	@echo "  usb           - Ask target, PlatformIO build + flash (https://platformio.org/)"
	@echo "  usb-ports     - List detected USB serial ports"
	@echo "  device-config - Show device_config.yaml and generate device_config.h"
	@echo "  coverage      - Generate HTML coverage report"
	@echo "  compile-commands - Merge sim + device compile_commands.json for the IDE"
	@echo "  compile       - Create build/compiled/arch_name with stripped/uglified output"
	@echo "  compile-clean - Clean compiled output"
	@echo "  compile-info  - Show compile configuration"
	@echo "  help          - Show this help"
	@echo ""
	@echo "Variables (can be set via env, command line, or build.mk):"
	@echo "  ARCH        - Target architecture (sim|esp32|esp8266|avr|rp2040)"
	@echo "  BUILD       - Build type (Debug|Release)"
	@echo "  JOBS        - Parallel jobs (default: auto)"
	@echo "  OUTPUT      - Output directory (default: build/ARCH/BUILD)"
	@echo "  FLAGS       - Extra C/C++ compiler flags"
	@echo "  CMAKE_OPTS  - Extra CMake options"
	@echo "  TEST_FILTER - Test filter (default: all)"
	@echo "  DEVICE      - USB target id (nodemcu|esp32|sim|...) skips the prompt"
	@echo "  PORT        - Serial port override (else device_config.yaml / auto)"
	@echo "  DEVICE_CONFIG - Path to YAML (default: device_config.yaml)"
	@echo ""
	@echo "Config files:"
	@echo "  build.mk            - optional make defaults (ARCH, BUILD, ...)"
	@echo "  device_config.yaml  - board port, LCD pins, buttons (required for make usb)"
	@echo "Example:"
	@echo "  make usb"
	@echo "  make usb DEVICE=nodemcu PORT=/dev/cu.wchusbserial1410"