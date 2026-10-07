#!/usr/bin/env python3
"""Unit tests for scripts/device_secrets.py."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from device_secrets import (  # noqa: E402
    SecretsError,
    c_escape,
    credentials_error,
    generate_secrets_header,
    link_key_error,
    load_wifi_secrets,
    parse_link_key,
    wifi_from_config,
    write_secrets_header,
)


class TestDeviceSecrets(unittest.TestCase):
    def test_ssid_and_password(self):
        cfg = {"wifi": {"ssid": "lab", "password": "s3cret"}}
        self.assertEqual(wifi_from_config(cfg), ("lab", "s3cret"))

    def test_user_pass_aliases(self):
        cfg = {"wifi": {"user": "cafe", "pass": "hunter2"}}
        self.assertEqual(wifi_from_config(cfg), ("cafe", "hunter2"))

    def test_ssid_wins_over_user(self):
        cfg = {"wifi": {"ssid": "real", "user": "alias", "password": "pw"}}
        self.assertEqual(wifi_from_config(cfg), ("real", "pw"))

    def test_quoted_yaml_keeps_hash(self):
        text = 'wifi:\n  ssid: "my net"\n  password: "p@ss#1"\n'
        with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as fh:
            fh.write(text)
            path = fh.name
        try:
            self.assertEqual(load_wifi_secrets(path), ("my net", "p@ss#1"))
        finally:
            Path(path).unlink(missing_ok=True)

    def test_missing_file_is_empty(self):
        self.assertEqual(load_wifi_secrets(ROOT / "no-such-device-secrets.yaml"), ("", ""))

    def test_placeholders_and_empty_are_rejected(self):
        self.assertIsNotNone(credentials_error("", "secret"))
        self.assertIsNotNone(credentials_error("lab", ""))
        self.assertIsNotNone(credentials_error("your-network", "your-password"))
        self.assertIsNone(credentials_error("lab", "secret"))
        self.assertIsNotNone(credentials_error("n" * 33, "secret"))
        self.assertIsNotNone(credentials_error("lab", "p" * 65))

    def test_header_escapes_and_omits_placeholders(self):
        header = generate_secrets_header("lab", 'a"b\\c')
        self.assertIn("#define ARDUBOT_WIFI_HAS_CREDS 1", header)
        self.assertIn('#define ARDUBOT_WIFI_SSID "lab"', header)
        self.assertIn('#define ARDUBOT_WIFI_PASSWORD "a\\"b\\\\c"', header)
        self.assertEqual(c_escape('a"b\\c'), 'a\\"b\\\\c')

        blank = generate_secrets_header("your-network", "your-password")
        self.assertIn("#define ARDUBOT_WIFI_HAS_CREDS 0", blank)
        self.assertNotIn("your-password", blank)
        self.assertNotIn("your-network", blank)

    def test_require_raises_without_writing_secrets(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "nested" / "device_secrets.h"
            with self.assertRaises(SecretsError):
                write_secrets_header(Path(tmp) / "missing.yaml", out, require=True)
            self.assertTrue(out.is_file())
            text = out.read_text(encoding="utf-8")
            self.assertIn("#define ARDUBOT_WIFI_HAS_CREDS 0", text)

            src = Path(tmp) / "device_secrets.yaml"
            src.write_text('wifi:\n  ssid: "lab"\n  password: "secret"\n', encoding="utf-8")
            write_secrets_header(src, out, require=True)
            baked = out.read_text(encoding="utf-8")
            self.assertIn("#define ARDUBOT_WIFI_HAS_CREDS 1", baked)
            self.assertIn('"lab"', baked)
            self.assertIn('"secret"', baked)

    def test_link_key_is_sixteen_bytes_and_zero_is_allowed(self):
        self.assertIsNone(link_key_error(1, "00" * 16))
        self.assertIsNotNone(link_key_error(1, "0011"))
        self.assertIsNotNone(link_key_error(1, ""))
        self.assertEqual(parse_link_key("00" * 16), bytes(16))
        header = generate_secrets_header("lab", "secret", 1, bytes(range(16)))
        self.assertIn("#define ARDUBOT_LINK_HAS_KEY 1", header)
        self.assertIn("#define ARDUBOT_LINK_KEY_ID 1", header)
        self.assertIn("0x00, 0x01, 0x02", header)
        blank = generate_secrets_header("lab", "secret")
        self.assertIn("#define ARDUBOT_LINK_HAS_KEY 0", blank)

    def test_require_link_rejects_a_missing_key(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "device_secrets.yaml"
            out = Path(tmp) / "device_secrets.h"
            src.write_text('wifi:\n  ssid: "lab"\n  password: "secret"\n', encoding="utf-8")
            with self.assertRaises(SecretsError):
                write_secrets_header(src, out, require=True, require_link=True)
            src.write_text(
                'wifi:\n  ssid: "lab"\n  password: "secret"\n'
                "link:\n  key_id: 1\n  key: \"00112233445566778899aabbccddeeff\"\n",
                encoding="utf-8",
            )
            write_secrets_header(src, out, require=True, require_link=True)
            self.assertIn("#define ARDUBOT_LINK_HAS_KEY 1", out.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
