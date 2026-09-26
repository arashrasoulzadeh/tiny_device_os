#!/usr/bin/env python3
"""Load gitignored device_secrets.yaml and emit a C header of WiFi credentials.

The YAML file is never committed. `make usb` passes --require so a board flash
stops until wifi.ssid and wifi.password are real values.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Any

from device_config import parse_simple_yaml

_PLACEHOLDER_SSID = "your-network"
_PLACEHOLDER_PASSWORD = "your-password"
_SSID_MAX = 32
_PASSWORD_MAX = 64

_EMPTY_HEADER = """\
/* Auto-generated — WiFi credentials are not set. Do not commit. */
#pragma once

#define ARDUBOT_WIFI_HAS_CREDS 0
#define ARDUBOT_WIFI_SSID ""
#define ARDUBOT_WIFI_PASSWORD ""
"""


class SecretsError(Exception):
    """device_secrets.yaml is missing or not usable for a device flash."""


def _as_str(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, bool):
        return "true" if value else ""
    return str(value)


def wifi_from_config(cfg: dict[str, Any]) -> tuple[str, str]:
    """Return (ssid, password). `user`/`pass` are aliases for ssid/password."""
    wifi = cfg.get("wifi")
    if not isinstance(wifi, dict):
        return "", ""
    ssid = wifi.get("ssid")
    if ssid is None:
        ssid = wifi.get("user", "")
    password = wifi.get("password")
    if password is None:
        password = wifi.get("pass", "")
    return _as_str(ssid), _as_str(password)


def load_wifi_secrets(path: str | Path) -> tuple[str, str]:
    path = Path(path)
    if not path.is_file():
        return "", ""
    cfg = parse_simple_yaml(path.read_text(encoding="utf-8"))
    return wifi_from_config(cfg)


def credentials_error(ssid: str, password: str) -> str | None:
    """None when both values are safe to bake into firmware."""
    if not ssid or not password:
        return (
            "set wifi.ssid and wifi.password in device_secrets.yaml "
            "(copy device_secrets.yaml.example)"
        )
    if ssid == _PLACEHOLDER_SSID or password == _PLACEHOLDER_PASSWORD:
        return (
            "replace the placeholder wifi.ssid and wifi.password "
            "in device_secrets.yaml"
        )
    if len(ssid) > _SSID_MAX or len(password) > _PASSWORD_MAX:
        return (
            f"wifi.ssid must be 1–{_SSID_MAX} characters and "
            f"wifi.password 1–{_PASSWORD_MAX}"
        )
    return None


def c_escape(text: str) -> str:
    out: list[str] = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ord(ch) < 32:
            out.append(f"\\x{ord(ch):02x}")
        else:
            out.append(ch)
    return "".join(out)


def generate_secrets_header(ssid: str, password: str) -> str:
    """C header. Unusable credentials are emitted as empty (not baked in)."""
    if credentials_error(ssid, password) is not None:
        return _EMPTY_HEADER
    return (
        "/* Auto-generated from device_secrets.yaml — do not edit or commit */\n"
        "#pragma once\n"
        "\n"
        "#define ARDUBOT_WIFI_HAS_CREDS 1\n"
        f'#define ARDUBOT_WIFI_SSID "{c_escape(ssid)}"\n'
        f'#define ARDUBOT_WIFI_PASSWORD "{c_escape(password)}"\n'
    )


def write_secrets_header(secrets_path: str | Path, out_path: str | Path, require: bool) -> None:
    ssid, password = load_wifi_secrets(secrets_path)
    err = credentials_error(ssid, password)
    header = generate_secrets_header(ssid, password)
    out = Path(out_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(header, encoding="utf-8")
    if require and err:
        raise SecretsError(err)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="ArdubotOS WiFi secrets helper")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_gen = sub.add_parser("gen-header", help="Write device_secrets.h")
    p_gen.add_argument("--secrets", default="device_secrets.yaml")
    p_gen.add_argument("-o", "--output", required=True)
    p_gen.add_argument(
        "--require",
        action="store_true",
        help="Exit 1 unless ssid and password are set",
    )

    p_check = sub.add_parser("check", help="Print whether WiFi credentials are set")
    p_check.add_argument("--secrets", default="device_secrets.yaml")

    args = parser.parse_args(argv)
    secrets = Path(args.secrets)

    if args.cmd == "check":
        if not secrets.is_file():
            print("wifi   : missing (copy device_secrets.yaml.example → device_secrets.yaml)")
            return 0
        ssid, password = load_wifi_secrets(secrets)
        err = credentials_error(ssid, password)
        if err:
            print(f"wifi   : not ready — {err}")
            return 0
        print(f"wifi   : {ssid} (password set)")
        return 0

    try:
        write_secrets_header(secrets, args.output, require=args.require)
    except SecretsError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
