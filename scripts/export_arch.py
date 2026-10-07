#!/usr/bin/env python3
"""Export a single-file, stripped amalgamation of the source ArdubotOS
would compile for one ARCH, test-compile it, and document it.

Writes exactly three files into build-<arch>/:
  - ardubot_<arch>.h   every header reachable for that arch, concatenated
  - ardubot_<arch>.c   every .c file reachable for that arch, concatenated
                       (local `#include "...h"` lines removed since their
                       content is now in the one header; system `#include
                       <...>` lines kept and de-duplicated up top)
  - README.md          Arduino IDE usage + Wi-Fi/key setup instructions

Both source files are comment/blank-line stripped before being written -
there is only ever one, already-minified copy, not a separate raw one.
After writing them, a best-effort `cc -c` test-compile is run (see
compile_test()) and its outcome is appended to the README and printed.

The source lists mirror the `target_sources(...)` calls in each
CMakeLists.txt rather than invoking CMake. Keep this in sync if those lists
change.
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from device_config import load_device_config, resolve_apps  # noqa: E402

VALID_ARCHES = ("sim", "esp32", "esp8266", "avr", "rp2040")

KERNEL_SOURCES = ["scheduler.c", "host_stack.c", "os_time.c", "alloc.c", "event.c", "power.c",
                   "power_governor.c"]
DRIVERS_SOURCES = [
    "driver.c", "module.c", "device_registry.c", "gpio_driver.c",
    "i2c_driver.c", "spi_driver.c", "uart_driver.c", "wifi_driver.c",
    "display_driver.c",
]
FS_SOURCES = ["vfs.c", "config_store.c", "ota.c", "ed25519.c"]
APPS_SOURCES = [
    "syscall.c", "app.c", "input.c", "ui.c", "app_utils.c", "app_kit.c",
    "ardubot_keys.c", "device_info.c", "stdlog.c", "app_ui.c",
    "ui/components/canvas.c", "ui/components/menu.c", "ui/components/catalog.c",
    "ui/components/screen.c", "ui/components/icons.c", "ui/components/status.c",
]

HAL_ARCH_SOURCES = {
    "sim": [
        "hal_gpio_sim.c", "hal_i2c_sim.c", "hal_spi_sim.c", "hal_uart_sim.c",
        "hal_display_sim.c", "hal_audio_sim.c", "hal_net_sim.c",
        "hal_storage_sim.c", "hal_power_sim.c", "hal_ble_sim.c",
        "hal_wifi_sim.c", "hal_adc_sim.c", "hal_pwm_sim.c", "hal_rtc_sim.c",
    ],
    "esp32": [
        "hal_gpio_esp32.c", "hal_i2c_esp32.c", "hal_spi_esp32.c",
        "hal_uart_esp32.c", "hal_display_esp32.c", "hal_audio_esp32.c",
        "hal_net_esp32.c", "hal_storage_esp32.c", "hal_power_esp32.c",
    ],
    "esp8266": [
        "hal_gpio_esp8266.c", "hal_i2c_esp8266.c", "hal_spi_esp8266.c",
        "hal_uart_esp8266.c", "hal_display_esp8266.c", "hal_audio_esp8266.c",
        "hal_net_esp8266.c", "hal_storage_esp8266.c", "hal_power_esp8266.c",
        "hal_wifi_esp8266.c", "hal_ble_esp8266.c", "hal_adc_esp8266.c",
        "hal_pwm_esp8266.c", "hal_rtc_esp8266.c",
    ],
    "avr": [
        "hal_gpio_avr.c", "hal_i2c_avr.c", "hal_spi_avr.c", "hal_uart_avr.c",
        "hal_display_avr.c", "hal_audio_avr.c", "hal_net_avr.c",
        "hal_storage_avr.c", "hal_power_avr.c",
    ],
    # rp2040 has no hal/arch/rp2040/ backend yet.
    "rp2040": [],
}

_ANY_INCLUDE_RE = re.compile(r'^\s*#include\s+[<"]([^>"]+)[>"]\s*$', re.MULTILINE)
_COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
_BLANKLINES_RE = re.compile(r"\n\s*\n+")
_LEADING_WS_RE = re.compile(r"^[ \t]+", re.MULTILINE)

ARCH_DEFINES = {
    "sim": [],
    "esp32": ["ARDUINO_ARCH_ESP32=1", "ESP_PLATFORM=1"],
    "esp8266": ["ARDUINO_ARCH_ESP8266=1"],
    "avr": ["ARDUINO_ARCH_AVR=1", "F_CPU=16000000UL"],
    "rp2040": [],
}


def strip_comments(text: str) -> str:
    text = _COMMENT_RE.sub("", text)
    text = _LEADING_WS_RE.sub("", text)
    text = _BLANKLINES_RE.sub("\n", text)
    return text.strip() + "\n"


def resolve_enabled_apps(arch: str, device_config: Path | None) -> list[str]:
    if arch == "sim" or device_config is None or not device_config.exists():
        pkg_dir = REPO_ROOT / "apps" / "stdapps"
        return sorted(p.name for p in pkg_dir.iterdir() if p.is_dir())
    cfg = load_device_config(device_config)
    enabled, _excluded = resolve_apps(cfg)
    return enabled


def collect_headers(arch: str) -> list[Path]:
    headers = sorted((REPO_ROOT / "hal" / "include").glob("hal_*.h"))

    arch_dir = REPO_ROOT / "hal" / "arch" / arch
    for name in HAL_ARCH_SOURCES.get(arch, []):
        h = (arch_dir / name).with_suffix(".h")
        if h.exists():
            headers.append(h)

    for base, names in (
        (REPO_ROOT / "kernel", KERNEL_SOURCES),
        (REPO_ROOT / "drivers", DRIVERS_SOURCES),
        (REPO_ROOT / "fs", FS_SOURCES),
        (REPO_ROOT / "apps", APPS_SOURCES),
    ):
        for name in names:
            h = (base / name).with_suffix(".h")
            if h.exists():
                headers.append(h)

    headers += sorted((REPO_ROOT / "kernel").glob("*.h"))
    headers += sorted((REPO_ROOT / "drivers").glob("*.h"))
    headers += sorted((REPO_ROOT / "fs").glob("*.h"))
    headers += sorted((REPO_ROOT / "apps").glob("*.h"))
    headers += sorted((REPO_ROOT / "apps" / "ui" / "components").glob("*.h"))
    return list(dict.fromkeys(headers))


def collect_sources(arch: str, device_config: Path | None) -> list[Path]:
    sources: list[Path] = []
    arch_dir = REPO_ROOT / "hal" / "arch" / arch
    sources += [arch_dir / n for n in HAL_ARCH_SOURCES.get(arch, [])]
    sources += [REPO_ROOT / "kernel" / n for n in KERNEL_SOURCES]
    sources += [REPO_ROOT / "drivers" / n for n in DRIVERS_SOURCES]
    sources += [REPO_ROOT / "fs" / n for n in FS_SOURCES]
    sources += [REPO_ROOT / "apps" / n for n in APPS_SOURCES]
    for app in resolve_enabled_apps(arch, device_config):
        sources += sorted((REPO_ROOT / "apps" / "stdapps" / app).glob("*.c"))
    return sources


def _banner(rel: Path) -> str:
    bar = "=" * 76
    return f"\n/* {bar}\n * {rel}\n * {bar} */\n"


def compile_test(arch: str, out_dir: Path, out_c: Path) -> tuple[bool, str]:
    """Best-effort `cc -c` of the amalgamated file. Real cross targets
    (esp32/esp8266/avr) need their vendor SDK (ESP-IDF, avr-libc) to fully
    resolve, which this host may not have - a failure there means "needs
    the real toolchain", not "the amalgamation is broken". sim is expected
    to actually compile.
    """
    if shutil.which("cc") is None:
        return False, "cc not found on PATH - skipped compile test"
    obj = out_dir / f"ardubot_{arch}.o"
    cmd = ["cc", "-c", "-I", str(out_dir), "-o", str(obj)]
    for d in ARCH_DEFINES.get(arch, []):
        cmd += ["-D", d]
    cmd.append(str(out_c))
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    finally:
        obj.unlink(missing_ok=True)
    if result.returncode == 0:
        return True, "cc -c succeeded"
    tail = "\n".join(result.stderr.strip().splitlines()[-20:])
    return False, f"cc -c failed (exit {result.returncode}):\n{tail}"


def write_readme(out_dir: Path, arch: str, compiled_ok: bool, compile_msg: str) -> None:
    status = "PASSED" if compiled_ok else "FAILED"
    readme = f"""# ArdubotOS - {arch} single-file build

This folder has the full ArdubotOS source for `ARCH={arch}` flattened to one
header/source pair, comments and blank lines stripped:

- `ardubot_{arch}.h` - every header this arch needs, concatenated
- `ardubot_{arch}.c` - every `.c` file this arch needs, concatenated

## Test-compile result

`cc -c` test-compile on this host: **{status}**

```
{compile_msg}
```

{"A FAILED result generally means a header this file needs wasn't pulled into the export. Two known causes: (1) for esp32/esp8266/avr, the vendor SDK (ESP-IDF / avr-libc) isn't installed on this host - the Arduino IDE / PlatformIO toolchain for the board does have those headers, so this is expected here and not a defect in the amalgamation; (2) `apps/app_framework.h` and `apps/app_ui.h` unconditionally include `sim/sim_gpio.h` regardless of target arch - a pre-existing coupling documented in `docs/agent-guide.md` ('New portable APIs still belong on the HAL, not as extra includes of sim/ from kernel or drivers'), not something this export script can fix without changing that coupling in the main repo. Check the error above against these two before assuming the amalgamation itself is wrong." if not compiled_ok else ""}

## Using in the Arduino IDE

1. Create a new sketch folder, e.g. `ardubot_{arch}/`.
2. Copy `ardubot_{arch}.h` and `ardubot_{arch}.c` into it. Arduino's builder
   only auto-compiles `.c`/`.cpp`/`.ino` files that live inside the sketch
   folder, so both files must sit next to your `.ino`, not in a subfolder.
3. Rename `ardubot_{arch}.c` to `ardubot_{arch}.cpp` if your board core needs
   C++ linkage for any Arduino library calls you add on top of it (the HAL
   backend in this file is plain C and builds either way).
4. In your sketch's `.ino`, add:
   ```cpp
   #include "ardubot_{arch}.h"
   ```
   and call this OS's entry points (kernel init / scheduler start - see the
   `scheduler.h` section inside `ardubot_{arch}.h` for the exact function
   names) from `setup()`; do not add your own `loop()` body that blocks, the
   scheduler expects to drive timing itself.
5. Tools > Board: pick the matching board for `{arch}` (e.g. an ESP32 Dev
   Module for `esp32`, a NodeMCU 1.0 (ESP-12E) for `esp8266`, an Arduino
   Mega 2560 for `avr`). Install that board's core via Boards Manager first
   if you haven't.

## Setting up keys (Wi-Fi credentials)

This single-file export has no Wi-Fi SSID/password baked in - the real
build normally generates those from a gitignored `device_secrets.yaml` (see
`device_secrets.yaml.example` and `scripts/device_secrets.py` in the main
repo) into a `device_secrets.h` with:

```c
#define ARDUBOT_WIFI_HAS_CREDS 1
#define ARDUBOT_WIFI_SSID "your-ssid"
#define ARDUBOT_WIFI_PASSWORD "your-password"
```

To use this exported file in the Arduino IDE, create that same
`device_secrets.h` yourself (with your own real SSID/password) next to
`ardubot_{arch}.h` in the sketch folder, and include it before
`ardubot_{arch}.h` in your `.ino`. Never commit that file - keep it out of
version control, same as `device_secrets.yaml` is in this repo's
`.gitignore`.

OTA firmware signing (`fs/ota.c`'s `ota_verify_and_apply`) takes its
ed25519 public key as a function argument at call time, not a compiled-in
constant, so there is nothing to bake into this export for OTA - pass your
own device's public key from whatever code calls that function.
"""
    (out_dir / "README.md").write_text(readme)


def export_arch(arch: str, out_dir: Path, device_config: Path | None) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    out_h = out_dir / f"ardubot_{arch}.h"
    out_c = out_dir / f"ardubot_{arch}.c"
    for existing in (out_h, out_c):
        existing.unlink(missing_ok=True)

    headers = collect_headers(arch)
    # Basenames of headers we've inlined, so any #include of one - whether
    # written with "quotes" or <angle brackets, e.g. hal/arch/sim's
    # `#include <power.h>` resolved via an extra -I rather than a relative
    # path - gets dropped instead of kept as a (nonexistent) system header.
    local_header_names = {h.name for h in headers}

    def _drop_if_local(m: re.Match) -> str:
        name = m.group(1).rsplit("/", 1)[-1]
        return "" if name in local_header_names else m.group()

    header_chunks = []
    for h in headers:
        if not h.exists():
            print(f"warning: missing {h}", file=sys.stderr)
            continue
        rel = h.relative_to(REPO_ROOT)
        text = _ANY_INCLUDE_RE.sub(_drop_if_local, h.read_text())
        header_chunks.append(_banner(rel) + text)
    out_h.write_text(strip_comments(f"#pragma once\n{''.join(header_chunks)}"))

    # System includes are left exactly where they are rather than hoisted
    # to the top - several (e.g. kernel/os_time.c, kernel/scheduler.c) sit
    # inside #ifdef _WIN32 guards, and hoisting them out would include
    # platform headers unconditionally and break the build on every other
    # host. Leaving duplicates in place is harmless: system headers carry
    # their own include guards.
    body_chunks = []
    for c in collect_sources(arch, device_config):
        if not c.exists():
            print(f"warning: missing {c}", file=sys.stderr)
            continue
        rel = c.relative_to(REPO_ROOT)
        text = _ANY_INCLUDE_RE.sub(_drop_if_local, c.read_text())
        body_chunks.append(_banner(rel) + text)

    out_c.write_text(strip_comments(f'#include "ardubot_{arch}.h"\n' + "".join(body_chunks)))

    print(f"Exported ArdubotOS source for ARCH={arch} to {out_h} and {out_c}")

    compiled_ok, compile_msg = compile_test(arch, out_dir, out_c)
    print(f"Test-compile: {'OK' if compiled_ok else 'FAILED'} - {compile_msg.splitlines()[0]}")

    write_readme(out_dir, arch, compiled_ok, compile_msg)
    print(f"Wrote {out_dir / 'README.md'}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", required=True, choices=VALID_ARCHES)
    parser.add_argument("--out", required=True, help="Output directory, e.g. build-esp32")
    parser.add_argument(
        "--device-config",
        default="device_config.yaml",
        help="device_config.yaml to resolve enabled stdapps from (real hardware only)",
    )
    args = parser.parse_args(argv)

    device_config = Path(args.device_config)
    export_arch(args.arch, Path(args.out), device_config if device_config.exists() else None)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
