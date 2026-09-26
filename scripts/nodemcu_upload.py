#!/usr/bin/env python3
"""NodeMCU upload for flaky CH340-on-macOS.

Never toggles DTR/RTS (kills this CH340). Manual FLASH+RST only.
Port "health" = node exists + open/close succeeds — no tcflush.
"""

from __future__ import annotations

import argparse
import glob
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

PORT_GLOBS = (
    "/dev/cu.usbserial*",
    "/dev/cu.wch*",
    "/dev/cu.SLAB*",
    "/dev/cu.usbmodem*",
)


def log(msg: str) -> None:
    print(msg, flush=True)


def list_usb_ports() -> list[str]:
    found: list[str] = []
    for pattern in PORT_GLOBS:
        found.extend(glob.glob(pattern))
    return sorted(set(found))


def find_esptool() -> list[str]:
    path = Path.home() / ".platformio/packages/tool-esptoolpy/esptool.py"
    if path.is_file():
        return [sys.executable, str(path)]
    return [sys.executable, "-m", "esptool"]


def port_is_openable(port: str) -> bool:
    """Open/close only — do not flush or touch DTR/RTS (CH340 dies on those)."""
    if not os.path.exists(port):
        return False
    try:
        import serial

        ser = serial.Serial()
        ser.port = port
        ser.baudrate = 115200
        ser.timeout = 0.05
        ser.dsrdtr = False  # don't assert DTR on open
        ser.rtscts = False
        ser.open()
        ser.close()
        return True
    except Exception as exc:
        log(f"  open {port}: {exc}")
        return False


def wait_for_port(preferred: str | None = None, timeout: float = 45.0) -> str | None:
    log("Waiting for USB serial (unplug/replug NodeMCU if needed)…")
    deadline = time.time() + timeout
    last_list: list[str] = []
    while time.time() < deadline:
        ports = list_usb_ports()
        if ports != last_list:
            last_list = ports
            log(f"  seen: {ports or '(none)'}")
        ordered = []
        if preferred:
            ordered.append(preferred)
        ordered.extend(p for p in ports if p not in ordered)
        for port in ordered:
            if port_is_openable(port):
                return port
        time.sleep(0.4)
    return None


def prompt_flash_mode() -> None:
    log("")
    log("Enter flash mode, then press Enter:")
    log("  1) Hold FLASH")
    log("  2) Tap RST, release RST")
    log("  3) Keep holding FLASH")
    log("  4) Press Enter here — release FLASH after Connecting… starts")
    log("")
    try:
        input()
    except EOFError:
        time.sleep(2.0)


def upload(port_arg: str, firmware: Path, baud: int) -> int:
    if not firmware.is_file():
        log(f"error: missing {firmware}")
        return 1

    preferred = None if not port_arg or port_arg.lower() == "auto" else port_arg
    current = wait_for_port(preferred, timeout=45.0)
    if not current:
        log(
            "error: no USB serial port.\n"
            "  1) Unplug the NodeMCU USB cable\n"
            "  2) Plug it back in\n"
            "  3) Run:  ls /dev/cu.usbserial*\n"
            "  4) make usb DEVICE=nodemcu"
        )
        return 1

    log(f"port ready: {current}")
    prompt_flash_mode()

    # Re-check after the pause (user may have bumped the cable)
    again = wait_for_port(current, timeout=20.0)
    if not again:
        log("error: port disappeared — unplug/replug and retry")
        return 1
    current = again

    cmd = [
        *find_esptool(),
        "--chip",
        "esp8266",
        "--port",
        current,
        "--baud",
        str(baud),
        "--before",
        "no_reset",
        "--after",
        "no_reset",
        "--connect-attempts",
        "20",
        "write_flash",
        "-fm",
        "dio",
        "-fs",
        "4MB",
        "0x0",
        str(firmware),
    ]
    log("+ " + " ".join(cmd))
    rc = subprocess.call(cmd)
    if rc == 0:
        log("Flash OK — press RST to run the firmware")
        return 0

    log(
        "Flash failed. Hold FLASH for the entire Connecting… phase, "
        "or ground D8, or try another cable/port."
    )
    return rc


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default=os.environ.get("UPLOAD_PORT", "auto"))
    p.add_argument("--firmware", default=str(ROOT / ".pio/build/nodemcu/firmware.bin"))
    p.add_argument("--baud", type=int, default=115200)
    args = p.parse_args()
    return upload(args.port, Path(args.firmware), args.baud)


if __name__ == "__main__":
    sys.exit(main())
