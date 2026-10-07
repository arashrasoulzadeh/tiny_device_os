#!/usr/bin/env python3
"""Link frames, AES-128-GCM, and the host session.

The wire format matches link/link_frame.h and link/link_session.h. USB is
one transport. open_tcp() speaks the same frames on a socket.
"""

from __future__ import annotations

import os
import select
import socket
import time
from typing import Callable

MAGIC = 0xAB07
VERSION = 1
MAX_PAYLOAD = 240
HEADER_LEN = 9

FLAG_ENCRYPTED = 0x08
CH_HELLO = 0
CH_CALL = 1
CH_RESULT = 2
CH_ERROR = 3
CH_EVENT = 4
CH_TTY = 5

METHOD = {
    "notify.post": 1,
    "app.message": 2,
    "app.event": 3,
    "input.key": 4,
}

TAG_METHOD = 1
TAG_APP = 2
TAG_TEXT = 3
TAG_BODY = 4
TAG_EXTENT = 5
TAG_DURATION = 6
TAG_PRESSED = 7
TAG_VERSION = 1
TAG_KEY_ID = 2
TAG_SESSION = 3
TAG_STATUS = 1
TAG_CODE = 1
TAG_ERRMSG = 2
TAG_SENSOR_KEY = 1
TAG_SENSOR_VALUE = 2

DIR_HOST = 0
DIR_DEVICE = 1

_SBOX = bytes([
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16,
])
_RCON = [0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36]


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def tlv_put(buf: bytearray, tag: int, value: bytes) -> None:
    if len(value) > 255:
        raise ValueError("tlv value is longer than 255 bytes")
    buf.append(tag)
    buf.append(len(value))
    buf.extend(value)


def tlv_get(buf: bytes, tag: int) -> bytes | None:
    i = 0
    while i + 2 <= len(buf):
        t = buf[i]
        n = buf[i + 1]
        if i + 2 + n > len(buf):
            return None
        if t == tag:
            return buf[i + 2 : i + 2 + n]
        i += 2 + n
    return None


def encode_frame(channel: int, msg_id: int, payload: bytes, flags: int = 0) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload too long")
    out = bytearray(HEADER_LEN + len(payload))
    out[0] = MAGIC & 0xFF
    out[1] = (MAGIC >> 8) & 0xFF
    out[2] = VERSION
    out[3] = flags
    out[4] = channel
    out[5] = msg_id & 0xFF
    out[6] = (msg_id >> 8) & 0xFF
    out[7] = len(payload) & 0xFF
    out[8] = (len(payload) >> 8) & 0xFF
    out[HEADER_LEN:] = payload
    c = crc16(out)
    out.append(c & 0xFF)
    out.append((c >> 8) & 0xFF)
    return bytes(out)


def decode_frame(raw: bytes) -> dict | None:
    if len(raw) < HEADER_LEN + 2 or raw[0] != (MAGIC & 0xFF) or raw[1] != ((MAGIC >> 8) & 0xFF):
        return None
    length = raw[7] | (raw[8] << 8)
    total = HEADER_LEN + length + 2
    if length > MAX_PAYLOAD or len(raw) < total:
        return None
    if crc16(raw[: HEADER_LEN + length]) != (raw[total - 2] | (raw[total - 1] << 8)):
        return None
    return {
        "version": raw[2],
        "flags": raw[3],
        "channel": raw[4],
        "msg_id": raw[5] | (raw[6] << 8),
        "payload": raw[HEADER_LEN : HEADER_LEN + length],
        "raw": raw[:total],
    }


class FrameStream:
    """Byte buffer that yields one frame at a time and resyncs on magic."""

    def __init__(self) -> None:
        self.buf = bytearray()

    def push(self, data: bytes) -> list[dict]:
        self.buf.extend(data)
        frames: list[dict] = []
        while True:
            start = 0
            while start + 1 < len(self.buf) and not (
                self.buf[start] == (MAGIC & 0xFF) and self.buf[start + 1] == ((MAGIC >> 8) & 0xFF)
            ):
                start += 1
            if start:
                del self.buf[:start]
            if len(self.buf) < HEADER_LEN:
                return frames
            length = self.buf[7] | (self.buf[8] << 8)
            if length > MAX_PAYLOAD:
                del self.buf[0]
                continue
            total = HEADER_LEN + length + 2
            if len(self.buf) < total:
                return frames
            frame = decode_frame(bytes(self.buf[:total]))
            if frame is None:
                del self.buf[0]
                continue
            frames.append(frame)
            del self.buf[:total]


def _xtime(x: int) -> int:
    return ((x << 1) ^ (0x1B if x & 0x80 else 0)) & 0xFF


def _expand_key(key: bytes) -> bytes:
    rk = bytearray(key)
    for i in range(4, 44):
        temp = bytearray(rk[(i - 1) * 4 : i * 4])
        if i % 4 == 0:
            t = temp[0]
            temp[0] = _SBOX[temp[1]]
            temp[1] = _SBOX[temp[2]]
            temp[2] = _SBOX[temp[3]]
            temp[3] = _SBOX[t]
            temp[0] ^= _RCON[i // 4]
        for j in range(4):
            rk.append(rk[(i - 4) * 4 + j] ^ temp[j])
    return bytes(rk)


def _aes_encrypt_block(round_key: bytes, block: bytes) -> bytes:
    state = bytearray(block)
    for i in range(16):
        state[i] ^= round_key[i]

    def add_round(rnd: int) -> None:
        off = rnd * 16
        for i in range(16):
            state[i] ^= round_key[off + i]

    def sub() -> None:
        for i in range(16):
            state[i] = _SBOX[state[i]]

    def shift() -> None:
        state[1], state[5], state[9], state[13] = state[5], state[9], state[13], state[1]
        state[2], state[6], state[10], state[14] = state[10], state[14], state[2], state[6]
        state[3], state[7], state[11], state[15] = state[15], state[3], state[7], state[11]

    def mix() -> None:
        for c in range(4):
            col = state[c * 4 : c * 4 + 4]
            a0, a1, a2, a3 = col
            state[c * 4] = _xtime(a0) ^ (_xtime(a1) ^ a1) ^ a2 ^ a3
            state[c * 4 + 1] = a0 ^ _xtime(a1) ^ (_xtime(a2) ^ a2) ^ a3
            state[c * 4 + 2] = a0 ^ a1 ^ _xtime(a2) ^ (_xtime(a3) ^ a3)
            state[c * 4 + 3] = (_xtime(a0) ^ a0) ^ a1 ^ a2 ^ _xtime(a3)

    for rnd in range(1, 10):
        sub()
        shift()
        mix()
        add_round(rnd)
    sub()
    shift()
    add_round(10)
    return bytes(state)


def _xor(a: bytearray, b: bytes) -> None:
    for i in range(16):
        a[i] ^= b[i]


def _shift_right(v: bytearray) -> None:
    lsb = v[15] & 1
    for i in range(15, 0, -1):
        v[i] = ((v[i] >> 1) | (v[i - 1] << 7)) & 0xFF
    v[0] = (v[0] >> 1) & 0xFF
    if lsb:
        v[0] ^= 0xE1


def _gcm_mul(x: bytearray, h: bytes) -> None:
    z = bytearray(16)
    v = bytearray(h)
    for i in range(16):
        for bit in range(8):
            if x[i] & (1 << (7 - bit)):
                _xor(z, v)
            _shift_right(v)
    x[:] = z


def aesgcm_encrypt(key: bytes, nonce: bytes, aad: bytes, pt: bytes) -> tuple[bytes, bytes]:
    rk = _expand_key(key)
    h = _aes_encrypt_block(rk, bytes(16))
    j0 = bytearray(16)
    j0[:12] = nonce
    j0[15] = 1
    counter = bytearray(j0)
    ct = bytearray()
    off = 0
    while off < len(pt):
        for i in range(15, 11, -1):
            counter[i] = (counter[i] + 1) & 0xFF
            if counter[i]:
                break
        stream = _aes_encrypt_block(rk, bytes(counter))
        chunk = pt[off : off + 16]
        ct.extend(bytes(a ^ b for a, b in zip(chunk, stream)))
        off += 16
    y = bytearray(16)
    if aad:
        y = _ghash_cont(h, y, aad)
    if ct:
        y = _ghash_cont(h, y, bytes(ct))
    lengths = (len(aad) * 8).to_bytes(8, "big") + (len(ct) * 8).to_bytes(8, "big")
    y = _ghash_cont(h, y, lengths)
    tag = bytes(a ^ b for a, b in zip(y, _aes_encrypt_block(rk, bytes(j0))))
    return bytes(ct), tag


def _ghash_cont(h: bytes, y: bytearray, data: bytes) -> bytearray:
    out = bytearray(y)
    off = 0
    while off < len(data):
        block = bytearray(16)
        chunk = data[off : off + 16]
        block[: len(chunk)] = chunk
        _xor(out, block)
        _gcm_mul(out, h)
        off += 16
    return out


def aesgcm_decrypt(key: bytes, nonce: bytes, aad: bytes, ct: bytes, tag: bytes) -> bytes | None:
    rk = _expand_key(key)
    h = _aes_encrypt_block(rk, bytes(16))
    j0 = bytearray(16)
    j0[:12] = nonce
    j0[15] = 1
    y = bytearray(16)
    if aad:
        y = _ghash_cont(h, y, aad)
    if ct:
        y = _ghash_cont(h, y, ct)
    lengths = (len(aad) * 8).to_bytes(8, "big") + (len(ct) * 8).to_bytes(8, "big")
    y = _ghash_cont(h, y, lengths)
    expect = bytes(a ^ b for a, b in zip(y, _aes_encrypt_block(rk, bytes(j0))))
    diff = 0
    for a, b in zip(expect, tag):
        diff |= a ^ b
    if diff:
        return None
    counter = bytearray(j0)
    pt = bytearray()
    off = 0
    while off < len(ct):
        for i in range(15, 11, -1):
            counter[i] = (counter[i] + 1) & 0xFF
            if counter[i]:
                break
        stream = _aes_encrypt_block(rk, bytes(counter))
        chunk = ct[off : off + 16]
        pt.extend(bytes(a ^ b for a, b in zip(chunk, stream)))
        off += 16
    return bytes(pt)


def _nonce(key_id: int, session: int, direction: int, counter: int) -> bytes:
    return bytes(
        [
            direction,
            key_id,
            session & 0xFF,
            (session >> 8) & 0xFF,
            counter & 0xFF,
            (counter >> 8) & 0xFF,
            (counter >> 16) & 0xFF,
            (counter >> 24) & 0xFF,
            (counter >> 32) & 0xFF,
            (counter >> 40) & 0xFF,
            (counter >> 48) & 0xFF,
            (counter >> 56) & 0xFF,
        ]
    )


class Session:
    def __init__(self, key: bytes, key_id: int, tx_dir: int) -> None:
        if len(key) != 16:
            raise ValueError("link key must be 16 bytes")
        self.key = bytes(key)
        self.key_id = key_id & 0xFF
        self.tx_dir = tx_dir
        self.session = 0
        self.tx_counter = 0
        self.rx_counter = 0
        self.open = False

    def encode_hello(self, session_id: int, msg_id: int) -> bytes:
        payload = bytearray()
        tlv_put(payload, TAG_VERSION, bytes([VERSION]))
        tlv_put(payload, TAG_KEY_ID, bytes([self.key_id]))
        tlv_put(payload, TAG_SESSION, bytes([session_id & 0xFF, (session_id >> 8) & 0xFF]))
        return encode_frame(CH_HELLO, msg_id, bytes(payload))

    def accept_hello(self, frame: dict, session_id: int = 0) -> None:
        if frame["channel"] != CH_HELLO or frame["flags"] & FLAG_ENCRYPTED:
            raise ValueError("not a hello")
        version = tlv_get(frame["payload"], TAG_VERSION)
        key_id = tlv_get(frame["payload"], TAG_KEY_ID)
        got = tlv_get(frame["payload"], TAG_SESSION)
        if version != bytes([VERSION]) or key_id != bytes([self.key_id]) or got is None or len(got) != 2:
            raise ValueError("key")
        got_session = got[0] | (got[1] << 8)
        if self.tx_dir == DIR_DEVICE:
            self.session = session_id
        elif got_session == 0:
            raise ValueError("session")
        else:
            self.session = got_session
        self.tx_counter = 0
        self.rx_counter = 0
        self.open = True

    def seal(self, channel: int, msg_id: int, pt: bytes) -> bytes:
        if not self.open:
            raise RuntimeError("session is not open")
        counter = self.tx_counter + 1
        body = counter.to_bytes(8, "little")
        length = 8 + len(pt) + 16
        header = bytes(
            [
                VERSION,
                FLAG_ENCRYPTED,
                channel,
                msg_id & 0xFF,
                (msg_id >> 8) & 0xFF,
                length & 0xFF,
                (length >> 8) & 0xFF,
            ]
        )
        aad = header + body
        nonce = _nonce(self.key_id, self.session, self.tx_dir, counter)
        ct, tag = aesgcm_encrypt(self.key, nonce, aad, pt)
        self.tx_counter = counter
        return encode_frame(channel, msg_id, body + ct + tag, FLAG_ENCRYPTED)

    def open_frame(self, frame: dict) -> dict:
        if not self.open or not (frame["flags"] & FLAG_ENCRYPTED) or len(frame["payload"]) < 24:
            raise ValueError("sealed frame")
        payload = frame["payload"]
        counter = int.from_bytes(payload[:8], "little")
        if counter == 0 or counter <= self.rx_counter:
            raise ValueError("replay")
        ct = payload[8:-16]
        tag = payload[-16:]
        direction = DIR_DEVICE if self.tx_dir == DIR_HOST else DIR_HOST
        length = len(payload)
        header = bytes(
            [
                frame["version"] or VERSION,
                frame["flags"],
                frame["channel"],
                frame["msg_id"] & 0xFF,
                (frame["msg_id"] >> 8) & 0xFF,
                length & 0xFF,
                (length >> 8) & 0xFF,
            ]
        )
        nonce = _nonce(self.key_id, self.session, direction, counter)
        pt = aesgcm_decrypt(self.key, nonce, header + payload[:8], ct, tag)
        if pt is None:
            raise ValueError("tag")
        self.rx_counter = counter
        return {
            "channel": frame["channel"],
            "msg_id": frame["msg_id"],
            "payload": pt,
        }


def encode_call(name: str, fields: dict) -> bytes:
    method = METHOD.get(name)
    if method is None:
        raise ValueError(f"unknown call {name}")
    payload = bytearray()
    tlv_put(payload, TAG_METHOD, bytes([method]))
    if name == "notify.post":
        title = str(fields.get("title", ""))
        if not title:
            raise ValueError("title")
        tlv_put(payload, TAG_TEXT, title.encode())
        body = fields.get("body")
        if body:
            tlv_put(payload, TAG_BODY, str(body).encode())
        extent = fields.get("extent", "band")
        tlv_put(payload, TAG_EXTENT, bytes([1 if extent == "full" else 0]))
        duration = int(fields.get("duration_ms", 0)) & 0xFFFFFFFF
        tlv_put(payload, TAG_DURATION, duration.to_bytes(4, "little"))
    elif name in ("app.message", "app.event"):
        app = str(fields.get("app", ""))
        text = str(fields.get("text", fields.get("name", "")))
        if not app or not text:
            raise ValueError("app")
        tlv_put(payload, TAG_APP, app.encode())
        tlv_put(payload, TAG_TEXT, text.encode())
        extra = fields.get("payload")
        if extra:
            raw = extra if isinstance(extra, bytes) else str(extra).encode()
            tlv_put(payload, TAG_BODY, raw[:64])
    elif name == "input.key":
        key = str(fields.get("key", ""))
        if not key:
            raise ValueError("key")
        tlv_put(payload, TAG_TEXT, key.encode())
        pressed = 1 if fields.get("pressed", True) else 0
        tlv_put(payload, TAG_PRESSED, bytes([pressed]))
    return bytes(payload)


def describe(plain: dict) -> str:
    payload = plain["payload"]
    channel = plain["channel"]
    if channel == CH_RESULT:
        return "ok"
    if channel == CH_ERROR:
        code = tlv_get(payload, TAG_CODE) or b"\x00\x00"
        text = tlv_get(payload, TAG_ERRMSG) or b""
        number = code[0] | ((code[1] << 8) if len(code) > 1 else 0)
        return f"error {number} {text.decode(errors='replace')}"
    if channel == CH_EVENT:
        key = tlv_get(payload, TAG_SENSOR_KEY) or b""
        raw = tlv_get(payload, TAG_SENSOR_VALUE) or b"\x00\x00\x00\x00"
        value = int.from_bytes(raw[:4], "little", signed=True)
        return f"sensor {key.decode(errors='replace')} {value}"
    return f"channel {channel}"


def _release_modem_lines(fd: int) -> None:
    """Release reset (RTS) and mark the CDC port open (DTR).

    Opening /dev/cu.usbmodem* lets macOS assert RTS, which holds the
    ESP32-C6 USB-JTAG controller in reset for as long as the port is open.
    """
    import fcntl
    import struct
    import termios

    dtr = getattr(termios, "TIOCM_DTR", 0)
    rts = getattr(termios, "TIOCM_RTS", 0)
    set_bits = getattr(termios, "TIOCMBIS", None)
    clear_bits = getattr(termios, "TIOCMBIC", None)
    if set_bits is None or clear_bits is None:
        return
    try:
        if rts:
            fcntl.ioctl(fd, clear_bits, struct.pack("I", rts))
        if dtr:
            fcntl.ioctl(fd, set_bits, struct.pack("I", dtr))
    except OSError:
        return


class Link:
    """Host or device session over a byte pipe."""

    def __init__(
        self,
        session: Session,
        write: Callable[[bytes], None],
        read: Callable[[int], bytes],
        fd: int | None = None,
        own_fd: bool = False,
    ) -> None:
        self.session = session
        self._write = write
        self._read = read
        self.fd = fd
        self._own_fd = own_fd
        self._stream = FrameStream()
        self._ready: list[dict] = []
        self._msg = 10
        self.notes: list[str] = []

    @classmethod
    def open_tcp(cls, sock: socket.socket, key: bytes, key_id: int = 1, role: str = "host", timeout: float = 2.0) -> "Link":
        sock.settimeout(timeout)

        def write(data: bytes) -> None:
            sock.sendall(data)

        def read(n: int) -> bytes:
            try:
                return sock.recv(n)
            except socket.timeout:
                return b""

        link = cls(
            Session(key, key_id, DIR_HOST if role == "host" else DIR_DEVICE),
            write,
            read,
            sock.fileno(),
        )
        link._handshake(role)
        return link

    @classmethod
    def open_usb(cls, port: str, key: bytes, key_id: int = 1) -> "Link":
        import termios

        fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
        attrs = termios.tcgetattr(fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        _release_modem_lines(fd)
        termios.tcflush(fd, termios.TCIOFLUSH)

        def write(data: bytes) -> None:
            os.write(fd, data)

        def read(n: int) -> bytes:
            ready, _, _ = select.select([fd], [], [], 0.4)
            if not ready:
                return b""
            return os.read(fd, n)

        link = cls(Session(key, key_id, DIR_HOST), write, read, fd, own_fd=True)
        link.port = port
        try:
            link._handshake("host")
        except Exception:
            link.close()
            raise
        return link

    def close(self) -> None:
        if self._own_fd and self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
        self.fd = None

    def _handshake(self, role: str) -> None:
        if role != "host":
            frame = self._next_frame()
            self.session.accept_hello(frame, session_id=1)
            self._write(self.session.encode_hello(self.session.session, 2))
            return
        # Opening the USB port can reset the chip. Keep offering Hello until
        # the firmware is up and answers, instead of one two-second wait.
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline:
            self._write(self.session.encode_hello(0, 1))
            try:
                frame = self._next_frame()
            except TimeoutError:
                continue
            if frame.get("channel") != CH_HELLO:
                continue
            self.session.accept_hello(frame)
            return
        where = getattr(self, "port", None) or "the board"
        raise TimeoutError(f"no reply from {where}")

    def _take_ready(self, frames: list[dict]) -> dict | None:
        if not frames:
            return None
        self._ready.extend(frames[1:])
        return frames[0]

    def _next_frame(self) -> dict:
        if self._ready:
            return self._ready.pop(0)
        for _ in range(50):
            found = self._take_ready(self._stream.push(b""))
            if found is not None:
                return found
            chunk = self._read(64)
            if not chunk:
                break
            found = self._take_ready(self._stream.push(chunk))
            if found is not None:
                return found
        raise TimeoutError("no link frame")

    def call(self, name: str, **fields: object) -> str:
        payload = encode_call(name, fields)
        self._msg += 1
        self._write(self.session.seal(CH_CALL, self._msg, payload))
        self.notes = []
        while True:
            plain = self.session.open_frame(self._next_frame())
            text = describe(plain)
            if plain["channel"] in (CH_RESULT, CH_ERROR):
                return text
            self.notes.append(text)

    def send_tty(self, data: bytes) -> None:
        view = data
        while view:
            chunk = view[:200]
            view = view[200:]
            self._msg += 1
            self._write(self.session.seal(CH_TTY, self._msg, chunk))

    def take_tty(self) -> bytes:
        """Decrypt any buffered TTY payloads. Other channels are consumed and dropped."""
        out = bytearray()

        def absorb(frames: list[dict]) -> None:
            for frame in frames:
                if not (frame["flags"] & FLAG_ENCRYPTED) or not self.session.open:
                    continue
                try:
                    plain = self.session.open_frame(frame)
                except ValueError:
                    continue
                if plain["channel"] == CH_TTY:
                    out.extend(plain["payload"])

        queued = self._ready
        self._ready = []
        absorb(queued)
        absorb(self._stream.push(b""))
        if self.fd is None:
            return bytes(out)
        ready, _, _ = select.select([self.fd], [], [], 0)
        if not ready:
            return bytes(out)
        absorb(self._stream.push(os.read(self.fd, 256)))
        return bytes(out)

    def poll(self) -> list[str]:
        """Read whatever is already buffered. Does not block past one select."""
        if self.fd is None:
            return []
        lines: list[str] = []
        ready, _, _ = select.select([self.fd], [], [], 0)
        if not ready:
            return lines
        chunk = os.read(self.fd, 256)
        for frame in self._stream.push(chunk):
            if frame["flags"] & FLAG_ENCRYPTED and self.session.open:
                try:
                    lines.append(describe(self.session.open_frame(frame)))
                except ValueError:
                    lines.append("dropped")
        return lines
