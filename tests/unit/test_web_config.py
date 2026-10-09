#!/usr/bin/env python3
"""Unit tests for secret redaction."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from web_server import redact_secrets  # noqa: E402


class TestSecretRedaction(unittest.TestCase):
    def test_redact_password(self):
        text = 'wifi_password = "mysecretpassword"'
        result = redact_secrets(text)
        self.assertIn("***REDACTED***", result)
        self.assertNotIn("mysecretpassword", result)

    def test_redact_ssid(self):
        text = 'wifi_ssid = "MyNetwork"'
        result = redact_secrets(text)
        self.assertIn("***REDACTED***", result)
        self.assertNotIn("MyNetwork", result)

    def test_redact_link_key(self):
        text = 'link_key = "0123456789abcdef0123456789abcdef"'
        result = redact_secrets(text)
        self.assertIn("***REDACTED***", result)
        self.assertNotIn("0123456789abcdef", result)

    def test_redact_hex_key_32(self):
        text = 'key = "0123456789abcdef0123456789abcdef"'
        result = redact_secrets(text)
        self.assertIn("***REDACTED_KEY***", result)

    def test_redact_hex_key_64(self):
        text = 'key = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"'
        result = redact_secrets(text)
        self.assertIn("***REDACTED_KEY***", result)

    def test_no_redaction_for_normal_text(self):
        text = 'building kernel...'
        result = redact_secrets(text)
        self.assertEqual(result, text)

    def test_multiple_secrets(self):
        text = 'ssid="MyNet" password="secret123" link_key="abcdef0123456789"'
        result = redact_secrets(text)
        self.assertIn("***REDACTED***", result)
        self.assertNotIn("MyNet", result)
        self.assertNotIn("secret123", result)
        self.assertNotIn("abcdef0123456789", result)


if __name__ == "__main__":
    unittest.main()