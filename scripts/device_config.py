#!/usr/bin/env python3
"""Load device_config.yaml, map NodeMCU pins, generate a C header, list serial ports.

Stdlib only — no PyYAML dependency. Supports the nested key:value schema used
by ArdubotOS device profiles.
"""

from __future__ import annotations

import argparse
import glob
import os
import re
import sys
from pathlib import Path
from typing import Any

# NodeMCU (ESP8266) silkscreen label → GPIO number
NODEMCU_PINS: dict[str, int] = {
    "D0": 16,
    "D1": 5,
    "D2": 4,
    "D3": 0,
    "D4": 2,
    "D5": 14,
    "D6": 12,
    "D7": 13,
    "D8": 15,
    "A0": 17,
    "RX": 3,
    "TX": 1,
}

KNOWN_TARGETS: list[dict[str, str]] = [
    {"id": "sim", "arch": "sim", "board": "sim", "label": "Host simulator (SDL2)", "pio_env": ""},
    {
        "id": "nodemcu",
        "arch": "esp8266",
        "board": "nodemcu",
        "label": "NodeMCU (ESP8266)",
        "pio_env": "nodemcu",
    },
    {
        "id": "esp32",
        "arch": "esp32",
        "board": "esp32-devkitc",
        "label": "ESP32 DevKit",
        "pio_env": "esp32dev",
    },
    {
        "id": "mega2560",
        "arch": "avr",
        "board": "mega2560",
        "label": "Arduino Mega2560",
        "pio_env": "megaatmega2560",
    },
    {
        "id": "pico",
        "arch": "rp2040",
        "board": "pico",
        "label": "Raspberry Pi Pico (RP2040)",
        "pio_env": "pico",
    },
]

USB_PORT_GLOBS = (
    "/dev/cu.usb*",
    "/dev/cu.wch*",
    "/dev/cu.SLAB*",
    "/dev/cu.usbmodem*",
    "/dev/tty.usb*",
    "/dev/tty.wch*",
    "/dev/tty.SLAB*",
    "/dev/ttyUSB*",
    "/dev/ttyACM*",
)


def _parse_scalar(raw: str) -> Any:
    text = raw.strip()
    if not text:
        return ""
    if text.startswith(("'", '"')) and text.endswith(text[0]) and len(text) >= 2:
        return text[1:-1]
    lower = text.lower()
    if lower in ("true", "yes", "on"):
        return True
    if lower in ("false", "no", "off"):
        return False
    if lower in ("null", "none", "~"):
        return None
    if re.fullmatch(r"0x[0-9a-fA-F]+", text):
        return int(text, 16)
    if re.fullmatch(r"-?\d+", text):
        return int(text)
    if "#" in text and not text.startswith("#"):
        text = text.split("#", 1)[0].rstrip()
        return _parse_scalar(text)
    return text


def parse_simple_yaml(text: str) -> dict[str, Any]:
    """Parse a restricted YAML subset (maps + list-of-maps) used by device_config."""
    root: dict[str, Any] = {}
    stack: list[tuple[int, Any]] = [(-1, root)]

    lines = text.splitlines()
    i = 0
    while i < len(lines):
        raw_line = lines[i]
        i += 1
        if not raw_line.strip() or raw_line.lstrip().startswith("#"):
            continue

        indent = len(raw_line) - len(raw_line.lstrip(" "))
        line = raw_line.strip()

        while len(stack) > 1 and indent <= stack[-1][0]:
            stack.pop()

        parent = stack[-1][1]

        if line.startswith("- "):
            item_body = line[2:].strip()
            if not isinstance(parent, list):
                raise ValueError(f"list item at unexpected place: {line!r}")
            if ":" in item_body:
                key, _, rest = item_body.partition(":")
                item = {key.strip(): _parse_scalar(rest)}
                parent.append(item)
                stack.append((indent, item))
            else:
                parent.append(_parse_scalar(item_body))
            continue

        if ":" not in line:
            raise ValueError(f"expected key: value, got {line!r}")

        key, _, rest = line.partition(":")
        key = key.strip()
        rest = rest.strip()
        if rest.startswith("#"):
            rest = ""

        if not isinstance(parent, dict):
            raise ValueError(f"mapping under non-dict: {key}")

        if rest != "":
            parent[key] = _parse_scalar(rest)
            continue

        j = i
        next_kind = "map"
        while j < len(lines):
            peek = lines[j]
            j += 1
            if not peek.strip() or peek.lstrip().startswith("#"):
                continue
            peek_indent = len(peek) - len(peek.lstrip(" "))
            if peek_indent <= indent:
                break
            next_kind = "list" if peek.lstrip().startswith("- ") else "map"
            break

        child: Any = [] if next_kind == "list" else {}
        parent[key] = child
        stack.append((indent, child))

    return root


def load_device_config(path: str | Path) -> dict[str, Any]:
    path = Path(path)
    text = path.read_text(encoding="utf-8")
    cfg = parse_simple_yaml(text)
    if "device" not in cfg:
        raise ValueError(f"{path}: missing top-level 'device' section")
    return cfg


def resolve_pin(label: str | int, board: str = "nodemcu") -> int:
    if isinstance(label, int):
        return label
    text = str(label).strip().upper()
    if text.startswith("GPIO"):
        return int(text[4:])
    if re.fullmatch(r"\d+", text):
        return int(text)
    table = NODEMCU_PINS if board in ("nodemcu", "esp8266", "") else NODEMCU_PINS
    if text not in table:
        raise ValueError(f"unknown pin label {label!r} for board {board!r}")
    return table[text]


def list_serial_ports() -> list[str]:
    found: list[str] = []
    for pattern in USB_PORT_GLOBS:
        found.extend(sorted(glob.glob(pattern)))
    # Prefer cu.* over tty.* on macOS (non-blocking open)
    cu = [p for p in found if "/cu." in p]
    if cu:
        return sorted(set(cu))
    return sorted(set(found))


def resolve_port(cfg: dict[str, Any], override: str | None = None) -> str | None:
    if override:
        return override
    device = cfg.get("device") or {}
    port = device.get("port", "auto")
    if port and str(port).lower() != "auto":
        return str(port)
    ports = list_serial_ports()
    return ports[0] if ports else None


def pin_conflict_warnings(cfg: dict[str, Any]) -> list[str]:
    warnings: list[str] = []
    board = (cfg.get("device") or {}).get("board", "nodemcu")
    used: dict[int, str] = {}

    lcd = cfg.get("lcd") or {}
    for key in ("scl", "sda", "sck", "cs", "dc", "rst", "bl"):
        if key not in lcd or lcd[key] is None:
            continue
        gpio = resolve_pin(lcd[key], board)
        label = f"lcd.{key}"
        if gpio in used:
            warnings.append(f"pin conflict: {label} and {used[gpio]} both map to GPIO{gpio}")
        else:
            used[gpio] = label

    for btn in (cfg.get("inputs") or {}).get("buttons") or []:
        if not isinstance(btn, dict) or "pin" not in btn:
            continue
        gpio = resolve_pin(btn["pin"], board)
        name = btn.get("name", "button")
        label = f"inputs.buttons.{name}"
        if gpio in used:
            warnings.append(
                f"pin conflict: {label} and {used[gpio]} both map to GPIO{gpio} "
                f"(LCD SCL on D6 cannot share a button)"
            )
        else:
            used[gpio] = label

    return warnings


def generate_header(cfg: dict[str, Any]) -> str:
    device = cfg.get("device") or {}
    lcd = cfg.get("lcd") or {}
    board = str(device.get("board", "nodemcu"))
    arch = str(device.get("arch", "esp8266"))

    lines = [
        "/* Auto-generated from device_config.yaml — do not edit */",
        "#pragma once",
        "",
        f'#define ARDUBOT_DEVICE_NAME "{device.get("name", board)}"',
        f'#define ARDUBOT_DEVICE_ARCH "{arch}"',
        f'#define ARDUBOT_DEVICE_BOARD "{board}"',
        "",
        f"#define ARDUBOT_LCD_WIDTH {int(lcd.get('width', 128))}",
        f"#define ARDUBOT_LCD_HEIGHT {int(lcd.get('height', 32))}",
        f'#define ARDUBOT_LCD_TYPE "{lcd.get("type", "ssd1306")}"',
        f'#define ARDUBOT_LCD_BUS "{lcd.get("bus", "i2c")}"',
        f"#define ARDUBOT_LCD_I2C_ADDR {int(lcd.get('address', 0x3C))}",
    ]

    if "scl" in lcd or "sck" in lcd:
        scl = lcd.get("scl", lcd.get("sck"))
        lines.append(f"#define ARDUBOT_LCD_SCL_GPIO {resolve_pin(scl, board)}")
    if "sda" in lcd:
        lines.append(f"#define ARDUBOT_LCD_SDA_GPIO {resolve_pin(lcd['sda'], board)}")

    buttons = (cfg.get("inputs") or {}).get("buttons") or []
    lines.append("")
    lines.append(f"#define ARDUBOT_BUTTON_COUNT {len(buttons)}")
    for idx, btn in enumerate(buttons):
        if not isinstance(btn, dict):
            continue
        gpio = resolve_pin(btn["pin"], board)
        name = str(btn.get("name", f"btn{idx}")).upper()
        name = re.sub(r"[^A-Z0-9_]", "_", name)
        active_low = 1 if btn.get("active_low", True) else 0
        lines.append(f"#define ARDUBOT_BTN_{name}_GPIO {gpio}")
        lines.append(f"#define ARDUBOT_BTN_{name}_ACTIVE_LOW {active_low}")
        lines.append(f"#define ARDUBOT_BTN_{idx}_GPIO {gpio}")
        lines.append(f"#define ARDUBOT_BTN_{idx}_ACTIVE_LOW {active_low}")

    lines.append("")
    return "\n".join(lines)


def write_header(cfg: dict[str, Any], out_path: str | Path) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(generate_header(cfg), encoding="utf-8")
    return out_path


def prompt_target(default_id: str | None = None) -> dict[str, str]:
    print("Compile / flash target:")
    for i, t in enumerate(KNOWN_TARGETS, start=1):
        mark = " (from device_config.yaml)" if default_id and t["id"] == default_id else ""
        print(f"  {i}) {t['id']:10} — {t['label']}{mark}")

    default_idx = 1
    if default_id:
        for i, t in enumerate(KNOWN_TARGETS, start=1):
            if t["id"] == default_id or t["arch"] == default_id or t["board"] == default_id:
                default_idx = i
                break

    while True:
        raw = input(f"Select [1-{len(KNOWN_TARGETS)}] (default {default_idx}): ").strip()
        if raw == "":
            return KNOWN_TARGETS[default_idx - 1]
        if raw.isdigit() and 1 <= int(raw) <= len(KNOWN_TARGETS):
            return KNOWN_TARGETS[int(raw) - 1]
        # also accept id string
        for t in KNOWN_TARGETS:
            if raw.lower() == t["id"] or raw.lower() == t["arch"]:
                return t
        print("Invalid selection, try again.")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="ArdubotOS device_config helper")
    parser.add_argument(
        "--config",
        default="device_config.yaml",
        help="Path to device_config.yaml",
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_show = sub.add_parser("show", help="Print parsed config summary")
    p_show.add_argument("--port", default=None)

    p_gen = sub.add_parser("gen-header", help="Write device_config.h")
    p_gen.add_argument(
        "-o",
        "--output",
        default="build/generated/device_config.h",
        help="Output header path",
    )

    p_ports = sub.add_parser("ports", help="List USB serial ports")

    args = parser.parse_args(argv)

    if args.cmd == "ports":
        ports = list_serial_ports()
        if not ports:
            print("No USB serial ports found.")
            print("On macOS, install the CH340/CP2102 driver if the NodeMCU is plugged in.")
            return 1
        for p in ports:
            print(p)
        return 0

    cfg = load_device_config(args.config)
    for w in pin_conflict_warnings(cfg):
        print(f"warning: {w}", file=sys.stderr)

    if args.cmd == "show":
        device = cfg["device"]
        lcd = cfg.get("lcd") or {}
        port = resolve_port(cfg, getattr(args, "port", None))
        print(f"device : {device.get('name')} arch={device.get('arch')} board={device.get('board')}")
        print(f"port   : {port or '(none detected — set device.port in device_config.yaml)'}")
        print(
            f"lcd    : {lcd.get('type')} {lcd.get('width')}x{lcd.get('height')} "
            f"scl={lcd.get('scl')} sda={lcd.get('sda')}"
        )
        for btn in (cfg.get("inputs") or {}).get("buttons") or []:
            print(f"button : {btn.get('name')} pin={btn.get('pin')}")
        return 0

    if args.cmd == "gen-header":
        path = write_header(cfg, args.output)
        print(f"Wrote {path}")
        return 0

    return 1


if __name__ == "__main__":
    sys.exit(main())
