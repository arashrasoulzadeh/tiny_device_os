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
#define ARDUBOT_LINK_HAS_KEY 0
#define ARDUBOT_LINK_KEY_ID 0
#define ARDUBOT_LINK_KEY {0}
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


def link_from_config(cfg: dict[str, Any]) -> tuple[int, str]:
    """Return (key_id, hex text). An absent link section is (0, "")."""
    link = cfg.get("link")
    if not isinstance(link, dict):
        return 0, ""
    raw = _as_str(link.get("key") or "").strip()
    key_id = link.get("key_id", 1 if raw else 0)
    try:
        return int(key_id), raw
    except (TypeError, ValueError):
        return -1, raw


def parse_link_key(raw: str) -> bytes | None:
    text = "".join(raw.split())
    if text.startswith(("0x", "0X")):
        text = text[2:]
    if len(text) != 32:
        return None
    try:
        return bytes.fromhex(text)
    except ValueError:
        return None


def link_key_error(key_id: int, raw: str) -> str | None:
    """None when `raw` is 16 bytes of hex. All-zero keys are allowed."""
    if not raw:
        return (
            "set link.key (32 hex digits) in device_secrets.yaml "
            "when notifications.forward_from_host is on"
        )
    if parse_link_key(raw) is None:
        return "link.key must be 16 bytes (32 hex digits)"
    if not 0 <= key_id <= 255:
        return "link.key_id must be 0–255"
    return None


def _link_macros(key_id: int, key: bytes) -> str:
    if len(key) != 16:
        return "#define ARDUBOT_LINK_HAS_KEY 0\n#define ARDUBOT_LINK_KEY_ID 0\n#define ARDUBOT_LINK_KEY {0}\n"
    body = ", ".join(f"0x{byte:02x}" for byte in key)
    return (
        "#define ARDUBOT_LINK_HAS_KEY 1\n"
        f"#define ARDUBOT_LINK_KEY_ID {key_id & 0xFF}\n"
        f"#define ARDUBOT_LINK_KEY {{{body}}}\n"
    )


def generate_secrets_header(
    ssid: str, password: str, link_key_id: int = 0, link_key: bytes = b""
) -> str:
    """C header. Unusable Wi-Fi credentials are emitted as empty (not baked in)."""
    if credentials_error(ssid, password) is not None:
        return _EMPTY_HEADER
    return (
        "/* Auto-generated from device_secrets.yaml — do not edit or commit */\n"
        "#pragma once\n"
        "\n"
        "#define ARDUBOT_WIFI_HAS_CREDS 1\n"
        f'#define ARDUBOT_WIFI_SSID "{c_escape(ssid)}"\n'
        f'#define ARDUBOT_WIFI_PASSWORD "{c_escape(password)}"\n'
        f"{_link_macros(link_key_id, link_key)}"
    )


def load_link_secrets(path: str | Path) -> tuple[int, bytes]:
    path = Path(path)
    if not path.is_file():
        return 0, b""
    cfg = parse_simple_yaml(path.read_text(encoding="utf-8"))
    key_id, raw = link_from_config(cfg)
    key = parse_link_key(raw) if raw else None
    if key is None or not 0 <= key_id <= 255:
        return 0, b""
    return key_id, key


def write_secrets_header(
    secrets_path: str | Path,
    out_path: str | Path,
    require: bool,
    require_link: bool = False,
) -> None:
    path = Path(secrets_path)
    ssid, password = "", ""
    key_id, raw = 0, ""
    if path.is_file():
        cfg = parse_simple_yaml(path.read_text(encoding="utf-8"))
        ssid, password = wifi_from_config(cfg)
        key_id, raw = link_from_config(cfg)
    err = credentials_error(ssid, password)
    link_err = link_key_error(key_id, raw) if require_link or raw else None
    key = parse_link_key(raw) if link_err is None and raw else b""
    if key is None:
        key = b""
    header = generate_secrets_header(ssid, password, key_id if key else 0, key)
    out = Path(out_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(header, encoding="utf-8")
    if require and err:
        raise SecretsError(err)
    if require_link and link_err:
        raise SecretsError(link_err)


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
