#!/usr/bin/env python3
"""Interactive USB build + flash via PlatformIO (https://platformio.org/).

Reads device_config.yaml for arch, serial port, LCD, and input pins.
Prompts which target to compile for unless --device / DEVICE is set.

Requires the `platformio` CLI on PATH (pip install platformio).

Usage:
  ./scripts/usb_flash.py
  ./scripts/usb_flash.py --device nodemcu
  make usb
  make usb DEVICE=nodemcu
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from device_config import (  # noqa: E402
    KNOWN_TARGETS,
    list_serial_ports,
    load_device_config,
    pin_conflict_warnings,
    prompt_target,
    resolve_port,
    write_header,
)

INSTALL_HINT = (
    "Install PlatformIO Core and ensure `platformio` is on PATH:\n"
    "  https://platformio.org/install/cli\n"
    "  pip install platformio"
)

ROSETTA_HINT = (
    "Apple Silicon needs Rosetta for the ESP8266 xtensa toolchain (x86_64 only).\n"
    "  softwareupdate --install-rosetta --agree-to-license\n"
    "Then retry: make usb DEVICE=nodemcu"
)


def find_platformio() -> list[str] | None:
    """Return argv to invoke the `platformio` CLI only (not `pio`)."""
    path = shutil.which("platformio")
    if path:
        return [path]
    try:
        subprocess.run(
            [sys.executable, "-m", "platformio", "--version"],
            capture_output=True,
            check=True,
        )
        return [sys.executable, "-m", "platformio"]
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def host_is_apple_silicon() -> bool:
    if sys.platform != "darwin":
        return False
    try:
        machine = subprocess.check_output(["uname", "-m"], text=True).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False
    return machine == "arm64"


def rosetta_available() -> bool:
    """True if this arm64 Mac can run x86_64 binaries via Rosetta."""
    try:
        rc = subprocess.call(
            ["arch", "-x86_64", "/usr/bin/true"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return rc == 0
    except FileNotFoundError:
        return False


def check_xtensa_toolchain_arch() -> str | None:
    """Return an error message if the installed xtensa toolchain cannot run."""
    gxx = (
        Path.home()
        / ".platformio"
        / "packages"
        / "toolchain-xtensa"
        / "bin"
        / "xtensa-lx106-elf-g++"
    )
    if not gxx.is_file():
        return None
    try:
        info = subprocess.check_output(["file", str(gxx)], text=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None
    if "x86_64" in info and host_is_apple_silicon() and not rosetta_available():
        return (
            f"{gxx} is an Intel (x86_64) binary and Rosetta is not installed.\n"
            f"{ROSETTA_HINT}"
        )
    # Probe execute — catches Bad CPU type even if file(1) output differs
    try:
        proc = subprocess.run(
            [str(gxx), "--version"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        if proc.returncode != 0 and "Bad CPU type" in (proc.stderr + proc.stdout):
            return ROSETTA_HINT
    except OSError as exc:
        if "Bad CPU type" in str(exc) or getattr(exc, "errno", None) == 86:
            return ROSETTA_HINT
    return None


def pio_env_for_target(target: dict[str, str], cfg: dict) -> str | None:
    device = cfg.get("device") or {}
    if device.get("pio_env"):
        return str(device["pio_env"])
    env = target.get("pio_env") or ""
    return env or None


def run_platformio(
    platformio: list[str],
    env: str,
    port: str | None,
    upload: bool,
    jobs: str | None = None,
) -> int:
    cmd = [*platformio, "run", "-e", env, "-d", str(ROOT)]
    if jobs:
        cmd.extend(["-j", str(jobs)])
    if upload:
        cmd.extend(["-t", "upload"])
        if port:
            cmd.extend(["--upload-port", port])
    print(f"+ {' '.join(cmd)}")
    return subprocess.call(cmd, cwd=ROOT)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Build and flash ArdubotOS over USB via PlatformIO"
    )
    parser.add_argument(
        "--config",
        default=str(ROOT / "device_config.yaml"),
        help="Path to device_config.yaml",
    )
    parser.add_argument(
        "--device",
        default=os.environ.get("DEVICE"),
        help="Target id (nodemcu, esp32, sim, ...) — skips interactive prompt",
    )
    parser.add_argument("--port", default=os.environ.get("PORT"), help="Serial port override")
    parser.add_argument("--yes", "-y", action="store_true", help="Non-interactive; use config defaults")
    parser.add_argument("--skip-build", action="store_true", help="Upload only")
    parser.add_argument("--jobs", default=None, help="Parallel jobs for platformio -j")
    args = parser.parse_args(argv)

    config_path = Path(args.config)
    if not config_path.is_file():
        print(f"error: missing {config_path}", file=sys.stderr)
        print("Create device_config.yaml with device.port, lcd, and inputs.", file=sys.stderr)
        return 1

    cfg = load_device_config(config_path)
    for w in pin_conflict_warnings(cfg):
        print(f"warning: {w}", file=sys.stderr)

    default_id = str(
        (cfg.get("device") or {}).get("name")
        or (cfg.get("device") or {}).get("board")
        or "nodemcu"
    )

    if args.device:
        target = None
        for t in KNOWN_TARGETS:
            if args.device.lower() in (t["id"], t["arch"], t["board"]):
                target = t
                break
        if target is None:
            print(f"error: unknown DEVICE={args.device}", file=sys.stderr)
            print("Valid: " + ", ".join(t["id"] for t in KNOWN_TARGETS), file=sys.stderr)
            return 1
    elif args.yes:
        target = next(
            t for t in KNOWN_TARGETS if t["id"] == default_id or t["board"] == default_id
        )
    else:
        if not sys.stdin.isatty():
            print(
                "error: no TTY for interactive select; pass --device=nodemcu or DEVICE=nodemcu",
                file=sys.stderr,
            )
            return 1
        target = prompt_target(default_id)

    arch = target["arch"]
    print(f"Target: {target['label']} (arch={arch})")

    header_path = ROOT / "build" / "generated" / "device_config.h"
    cfg_for_header = dict(cfg)
    cfg_for_header["device"] = dict(cfg.get("device") or {})
    cfg_for_header["device"]["arch"] = arch
    cfg_for_header["device"]["board"] = target["board"]
    write_header(cfg_for_header, header_path)
    print(f"Generated {header_path.relative_to(ROOT)}")

    if arch == "sim":
        if not args.skip_build:
            return subprocess.call(["make", "run", "ARCH=sim"], cwd=ROOT)
        print("sim selected — nothing to flash over USB.")
        return 0

    env_name = pio_env_for_target(target, cfg)
    if not env_name:
        print(f"error: no PlatformIO env mapped for {target['id']}", file=sys.stderr)
        return 1

    platformio = find_platformio()
    if not platformio:
        print("error: `platformio` not found on PATH.", file=sys.stderr)
        print(INSTALL_HINT, file=sys.stderr)
        return 1

    if arch in ("esp8266",) and host_is_apple_silicon():
        toolchain_err = check_xtensa_toolchain_arch()
        if toolchain_err:
            print(f"error: {toolchain_err}", file=sys.stderr)
            return 1
        if not rosetta_available():
            # Toolchain not downloaded yet, but build will need Rosetta
            print("warning: ESP8266 toolchain is x86_64; Rosetta required on Apple Silicon.")
            print(f"  {ROSETTA_HINT.split(chr(10))[1].strip()}")

    ver = subprocess.run([*platformio, "--version"], capture_output=True, text=True)
    if ver.stdout.strip():
        print(f"PlatformIO: {ver.stdout.strip()}")

    port = resolve_port(cfg, args.port)
    if port and not os.path.exists(port):
        print(f"warning: configured port {port} is missing — will re-scan USB")
        port = None
    if not port:
        ports = list_serial_ports()
        print("error: no USB serial port found.", file=sys.stderr)
        print(
            "The CH340 device is not visible to macOS right now.",
            file=sys.stderr,
        )
        print(
            "Unplug the NodeMCU, wait 2s, plug into a Mac USB port (not only a hub),",
            file=sys.stderr,
        )
        print("then run:  ls /dev/cu.usb*   and  make usb DEVICE=nodemcu", file=sys.stderr)
        if ports:
            print("Detected:", ", ".join(ports), file=sys.stderr)
        print("Continuing with build only (no upload)...")
        return run_platformio(platformio, env_name, None, upload=False, jobs=args.jobs)

    print(f"Serial port: {port}")
    print(f"PlatformIO env: {env_name}")
    if arch == "esp8266":
        print(
            "Note: this CH340 cannot use Arduino auto-reset (DTR kills the port). "
            "Uploader will ask you to hold FLASH + tap RST."
        )
    return run_platformio(platformio, env_name, port, upload=True, jobs=args.jobs)


if __name__ == "__main__":
    sys.exit(main())
