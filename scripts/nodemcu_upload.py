#!/usr/bin/env python3
"""NodeMCU upload.

macOS CH340 ports die if pyserial or esptool toggles DTR, so that host
uses a manual FLASH+RST sequence and --before no_reset. Linux tries
esptool's normal reset first. If that never syncs, it asks for the same
FLASH+RST sequence and retries with --before no_reset.

A NodeMCU CH340 is /dev/ttyUSB* (or cu.usbserial/wch on macOS). /dev/ttyACM*
is usually a different board's native USB (the ESP32-C6). When both are
plugged in, the UART adapter is tried first unless the caller named a
UART port explicitly.
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
    "/dev/ttyUSB*",
    "/dev/ttyACM*",
    "/dev/tty.usbserial*",
    "/dev/tty.wch*",
    "/dev/tty.SLAB*",
    "/dev/tty.usbmodem*",
)


def esptool_reset_mode(platform: str | None = None) -> tuple[str, str]:
    """Return esptool --before/--after. Linux resets the chip; macOS must not."""
    plat = sys.platform if platform is None else platform
    if plat.startswith("linux"):
        return "default_reset", "hard_reset"
    return "no_reset", "no_reset"


def reset_attempts(platform: str | None = None) -> list[tuple[str, str]]:
    """Linux auto-reset, then a manual FLASH+RST retry. macOS is manual only."""
    before, after = esptool_reset_mode(platform)
    if before == "no_reset":
        return [("no_reset", "no_reset")]
    return [("default_reset", "hard_reset"), ("no_reset", "no_reset")]


_UART_PREFIXES = (
    "ttyUSB",
    "tty.usbserial",
    "tty.wch",
    "tty.SLAB",
    "cu.usbserial",
    "cu.wch",
    "cu.SLAB",
)


def is_uart_adapter(port: str) -> bool:
    """CH340 / CP2102 / FTDI style nodes. Native USB (ttyACM, usbmodem) is not."""
    return os.path.basename(port).startswith(_UART_PREFIXES)


def rank_ports(ports: list[str], preferred: str | None = None) -> list[str]:
    """Order candidates so a NodeMCU UART wins over another board's ttyACM.

    An explicit UART `preferred` stays first. A preferred ttyACM is used
    only when no UART adapter is present.
    """
    unique: list[str] = []
    for port in ports:
        if port not in unique:
            unique.append(port)
    uart = sorted(p for p in unique if is_uart_adapter(p))
    native = sorted(p for p in unique if not is_uart_adapter(p))
    if preferred and is_uart_adapter(preferred):
        rest_uart = [p for p in uart if p != preferred]
        rest_native = [p for p in native if p != preferred]
        return [preferred, *rest_uart, *rest_native]
    if preferred and preferred in native and not uart:
        return [preferred, *[p for p in native if p != preferred]]
    return uart + native


def port_list_hint(platform: str | None = None) -> str:
    plat = sys.platform if platform is None else platform
    if plat.startswith("linux"):
        return "ls /dev/ttyUSB* /dev/ttyACM*"
    return "ls /dev/cu.usbserial*"


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
        ordered = rank_ports(ports, preferred)
        for port in ordered:
            if port_is_openable(port):
                if preferred and port != preferred and is_uart_adapter(port):
                    log(
                        f"Using {port} for NodeMCU (CH340). "
                        f"{preferred} is left for the other board."
                    )
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


def esptool_cmd(
    port: str, firmware: Path, baud: int, before: str, after: str, attempts: int
) -> list[str]:
    return [
        *find_esptool(),
        "--chip",
        "esp8266",
        "--port",
        port,
        "--baud",
        str(baud),
        "--before",
        before,
        "--after",
        after,
        "--connect-attempts",
        str(attempts),
        "write_flash",
        "-fm",
        "dio",
        "-fs",
        "4MB",
        "0x0",
        str(firmware),
    ]


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
            f"  3) Run:  {port_list_hint()}\n"
            "  4) make usb DEVICE=nodemcu"
        )
        return 1

    log(f"port ready: {current}")
    attempts = reset_attempts()
    rc = 1
    for index, (before, after) in enumerate(attempts):
        if before == "no_reset":
            if index > 0:
                log("Auto-reset did not connect. Switching to manual flash mode.")
            prompt_flash_mode()
            again = wait_for_port(current, timeout=20.0)
            if not again:
                log("error: port disappeared — unplug/replug and retry")
                return 1
            current = again
        cmd = esptool_cmd(
            current,
            firmware,
            baud,
            before,
            after,
            3 if before == "default_reset" else 20,
        )
        log("+ " + " ".join(cmd))
        rc = subprocess.call(cmd)
        if rc == 0:
            if before == "no_reset":
                log("Flash OK — press RST to run the firmware")
            else:
                log("Flash OK")
            return 0

    log(
        "Flash failed. Hold FLASH for the entire Connecting… phase, "
        "or ground D8, or try another cable/port."
    )
    return rc


def main() -> int:
    from venv_exec import prefer_project_venv

    prefer_project_venv()
    p = argparse.ArgumentParser()
    p.add_argument("--port", default=os.environ.get("UPLOAD_PORT", "auto"))
    p.add_argument("--firmware", default=str(ROOT / ".pio/build/nodemcu/firmware.bin"))
    p.add_argument("--baud", type=int, default=115200)
    args = p.parse_args()
    return upload(args.port, Path(args.firmware), args.baud)


if __name__ == "__main__":
    sys.exit(main())
