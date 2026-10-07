#!/usr/bin/env python3
"""Terminal for the device shell.

After a successful flash the host is only the screen and keyboard.
The board echoes, edits the line, and runs commands. Ctrl+] closes
the port. Ctrl+C and Ctrl+Z are sent to the shell.
"""

from __future__ import annotations

import os
import select
import sys

from link_codec import Link

EXIT_BYTE = 0x1D


def tty_local_action(byte: int) -> str:
    """Ctrl+] leaves the host. Every other byte is sent to the board."""
    if byte == EXIT_BYTE:
        return "exit"
    return "send"


def run_monitor(port: str, key: bytes, key_id: int = 1) -> int:
    """Raw terminal on `port` until Ctrl+]."""
    import termios
    import tty

    link = Link.open_usb(port, key, key_id)
    stdin = sys.stdin.fileno()
    saved = termios.tcgetattr(stdin) if sys.stdin.isatty() else None
    print("link tty — Ctrl+] exits, Ctrl+Z leaves an app, Ctrl+C cancels", flush=True)
    try:
        if saved is not None:
            tty.setraw(stdin)
        pending = link.take_tty()
        if pending:
            sys.stdout.buffer.write(pending)
            sys.stdout.buffer.flush()
        while True:
            watch = [stdin]
            if link.fd is not None:
                watch.append(link.fd)
            ready, _, _ = select.select(watch, [], [])
            if link.fd in ready:
                text = link.take_tty()
                if text:
                    sys.stdout.buffer.write(text)
                    sys.stdout.buffer.flush()
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
