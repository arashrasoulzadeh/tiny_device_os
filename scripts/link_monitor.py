#!/usr/bin/env python3
"""Terminal for the device shell.

After a successful flash the host is only the screen and keyboard.
The board echoes, edits the line, and runs commands. Ctrl+] closes
the port. Ctrl+C and Ctrl+Z are sent to the shell.
"""

from __future__ import annotations

import argparse
import os
import select
import sys
from pathlib import Path

from link_codec import Link

ROOT = Path(__file__).resolve().parents[1]

EXIT_BYTE = 0x1D


def tty_local_action(byte: int) -> str:
    """Ctrl+] leaves the host. Every other byte is sent to the board."""
    if byte == EXIT_BYTE:
        return "exit"
    return "send"


CLOSE_BYTE = 0x04


def tty_display(data: bytes) -> bytes:
    """Raw mode does not turn a newline into a return to column 0."""
    out = bytearray()
    prev = 0
    for byte in data:
        if byte == 0x0A and prev != 0x0D:
            out.append(0x0D)
        out.append(byte)
        prev = byte
    return bytes(out)


def tty_present(data: bytes) -> tuple[bytes, bool]:
    """Show shell text. EOT from `exit` or `close` drops the port."""
    close = CLOSE_BYTE in data
    visible = data.replace(bytes([CLOSE_BYTE]), b"")
    return tty_display(visible), close


def run_monitor(port: str, key: bytes, key_id: int = 1) -> int:
    """Raw terminal on `port` until Ctrl+]."""
    import termios
    import tty

    link = Link.open_usb(port, key, key_id)
    stdin = sys.stdin.fileno()
    saved = termios.tcgetattr(stdin) if sys.stdin.isatty() else None
    print("link tty — exit closes, Ctrl+] exits, Ctrl+Z leaves an app, Ctrl+C cancels", flush=True)
    try:
        if saved is not None:
            tty.setraw(stdin)
        pending = link.take_tty()
        if pending:
            shown, hangup = tty_present(pending)
            if shown:
                sys.stdout.buffer.write(shown)
                sys.stdout.buffer.flush()
            if hangup:
                return 0
        while True:
            watch = [stdin]
            if link.fd is not None:
                watch.append(link.fd)
            ready, _, _ = select.select(watch, [], [])
            if link.fd in ready:
                text = link.take_tty()
                if text:
                    shown, hangup = tty_present(text)
                    if shown:
                        sys.stdout.buffer.write(shown)
                        sys.stdout.buffer.flush()
                    if hangup:
                        break
            if stdin in ready:
                raw = os.read(stdin, 64)
                send = bytearray()
                leave = False
                for byte in raw:
                    if tty_local_action(byte) == "exit":
                        leave = True
                        break
                    send.append(byte)
                if send:
                    link.send_tty(bytes(send))
                if leave:
                    break
    except KeyboardInterrupt:
        pass
    finally:
        if saved is not None:
            termios.tcsetattr(stdin, termios.TCSANOW, saved)
        link.close()
        print()
    return 0


PROMPT = b"ardubot$ "


def tty_command_result(payload: bytes, command: str) -> bytes:
    """Command output, without the echoed line or the next prompt."""
    end = payload.rfind(PROMPT)
    if end >= 0:
        payload = payload[:end]
    echo = command.encode() + b"\n"
    if payload.startswith(echo):
        payload = payload[len(echo) :]
    return payload.replace(b"\r\n", b"\n").replace(b"\r", b"")


def _read_until_prompt(link: Link, timeout: float) -> bytes:
    import time

    buf = bytearray()
    deadline = time.time() + timeout
    while time.time() < deadline:
        chunk = link.take_tty()
        if chunk:
            buf.extend(chunk)
            if PROMPT in buf:
                return bytes(buf)
            continue
        if link.fd is None:
            break
        wait = min(0.2, max(0.0, deadline - time.time()))
        select.select([link.fd], [], [], wait)
    return bytes(buf)


def run_once(port: str, key: bytes, key_id: int, command: str, timeout: float = 5.0) -> int:
    """Send one shell line, print its output, and close the port."""
    link = Link.open_usb(port, key, key_id)
    try:
        _read_until_prompt(link, timeout)
        link.send_tty(command.encode() + b"\n")
        payload = _read_until_prompt(link, timeout)
    finally:
        link.close()
    finished = PROMPT in payload
    sys.stdout.buffer.write(tty_command_result(payload, command))
    sys.stdout.buffer.flush()
    if not finished:
        print("error: command did not return a prompt", file=sys.stderr)
        return 1
    return 0


def session_target(config_path: str, secrets_path: str, port: str | None = None) -> tuple[str, int, bytes]:
    """Port and link key for a shell that is already on the board."""
    from device_config import load_device_config, resolve_port
    from device_secrets import load_link_secrets

    cfg = load_device_config(config_path)
    found = resolve_port(cfg, port)
    if not found:
        raise SystemExit("error: no USB serial port found")
    key_id, key = load_link_secrets(secrets_path)
    if not key:
        raise SystemExit("error: set link.key in device_secrets.yaml")
    return found, key_id, key


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Reconnect the device shell without flashing")
    parser.add_argument("--config", default="device_config.yaml")
    parser.add_argument("--secrets", default=str(ROOT / "device_secrets.yaml"))
    parser.add_argument("--port", default=None)
    parser.add_argument("--run", nargs=argparse.REMAINDER, help="Run one command and exit")
    args = parser.parse_args(argv)
    port, key_id, key = session_target(args.config, args.secrets, args.port)
    try:
        return _run(args, port, key, key_id)
    except TimeoutError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


def _run(args: argparse.Namespace, port: str, key: bytes, key_id: int) -> int:
    print(f"connecting {port}", file=sys.stderr)
    if args.run is not None:
        command = " ".join(args.run).strip()
        if not command:
            print("usage: make tty-run <command> [args...]", file=sys.stderr)
            return 2
        return run_once(port, key, key_id, command)
    return run_monitor(port, key, key_id)


if __name__ == "__main__":
    sys.exit(main())
