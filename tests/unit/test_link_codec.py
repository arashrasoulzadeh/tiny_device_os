#!/usr/bin/env python3
"""Shared vectors for the Python link codec, plus a socket-pair session."""

from __future__ import annotations

import socket
import sys
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from link_codec import (  # noqa: E402
    CH_CALL,
    CH_TTY,
    DIR_DEVICE,
    DIR_HOST,
    Link,
    Session,
    aesgcm_decrypt,
    aesgcm_encrypt,
    crc16,
    decode_frame,
)
from link_monitor import tty_local_action  # noqa: E402


class CodecTests(unittest.TestCase):
    def test_crc_and_nist_gcm(self):
        self.assertEqual(crc16(b"123456789"), 0x29B1)
        key = bytes(16)
        nonce = bytes(12)
        _ct, tag = aesgcm_encrypt(key, nonce, b"", b"")
        self.assertEqual(tag.hex(), "58e2fccefa7e3061367f1d57a4e7455a")
        ct, tag = aesgcm_encrypt(key, nonce, b"", bytes(16))
        self.assertEqual(ct.hex(), "0388dace60b6a392f328c2b971b2fe78")
        self.assertEqual(tag.hex(), "ab6e47d42cec13bdf53a67b21257bddf")
        self.assertIsNone(aesgcm_decrypt(key, nonce, b"", ct, bytes(tag[0] ^ 1) + tag[1:]))

    def test_tty_bytes_roundtrip_and_ctrl_bracket_exits(self):
        key = bytes(range(16))
        host = Session(key, 1, DIR_HOST)
        device = Session(key, 1, DIR_DEVICE)
        hello = decode_frame(host.encode_hello(0, 1))
        assert hello is not None
        device.accept_hello(hello, session_id=1)
        reply = decode_frame(device.encode_hello(device.session, 2))
        assert reply is not None
        host.accept_hello(reply)
        wire = host.seal(CH_TTY, 3, b"sensors\n\x1a\x03")
        frame = decode_frame(wire)
        assert frame is not None
        plain = device.open_frame(frame)
        self.assertEqual(plain["channel"], CH_TTY)
        self.assertEqual(plain["payload"], b"sensors\n\x1a\x03")
        self.assertEqual(tty_local_action(0x1D), "exit")
        self.assertEqual(tty_local_action(0x03), "send")
        self.assertEqual(tty_local_action(0x1A), "send")

    def test_prompt_survives_hello_and_other_channels_are_dropped(self):
        key = bytes(range(16))
        device = Session(key, 1, DIR_DEVICE)
        opener = Session(key, 1, DIR_HOST)
        hello = decode_frame(opener.encode_hello(0, 1))
        assert hello is not None
        device.accept_hello(hello, session_id=1)
        blob = device.encode_hello(1, 2) + device.seal(CH_TTY, 3, b"hi") + device.seal(2, 4, b"ok")
        chunks = [blob]

        def read(_n: int) -> bytes:
            return chunks.pop(0) if chunks else b""

        host = Link(Session(key, 1, DIR_HOST), lambda _data: None, read, fd=None)
        host._handshake("host")
        self.assertEqual(host.take_tty(), b"hi")

    def test_tcp_session_roundtrip(self):
        host_sock, device_sock = socket.socketpair()
        key = bytes(range(16))
        seen: dict = {}

        def device() -> None:
            link = Link.open_tcp(device_sock, key, 1, role="device")
            plain = link.session.open_frame(link._next_frame())
            seen["channel"] = plain["channel"]
            seen["payload"] = plain["payload"]
            from link_codec import TAG_STATUS, tlv_put

            reply = bytearray()
            tlv_put(reply, TAG_STATUS, bytes([0]))
            link._write(link.session.seal(2, 4, bytes(reply)))

        thread = threading.Thread(target=device)
        thread.start()
        host = Link.open_tcp(host_sock, key, 1, role="host")
        try:
            text = host.call("notify.post", title="hello world", extent="band", duration_ms=0)
            thread.join(3)
            self.assertEqual(text, "ok")
            self.assertEqual(seen["channel"], CH_CALL)
            self.assertIn(b"hello world", seen["payload"])
        finally:
            host.close()
            host_sock.close()
            device_sock.close()


if __name__ == "__main__":
    unittest.main()
