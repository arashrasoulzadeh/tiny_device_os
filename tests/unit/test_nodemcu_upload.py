#!/usr/bin/env python3
"""Linux serial-port names and esptool reset policy for the NodeMCU uploader."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from nodemcu_upload import PORT_GLOBS, esptool_reset_mode, port_list_hint  # noqa: E402


class TestNodemcuUploadLinux(unittest.TestCase):
    def test_linux_serial_nodes_are_scanned(self):
        self.assertIn("/dev/ttyUSB*", PORT_GLOBS)
        self.assertIn("/dev/ttyACM*", PORT_GLOBS)
        self.assertIn("/dev/cu.usbserial*", PORT_GLOBS)

    def test_linux_uses_esptool_reset(self):
        self.assertEqual(esptool_reset_mode("linux"), ("default_reset", "hard_reset"))
        self.assertIn("ttyUSB", port_list_hint("linux"))

    def test_macos_does_not_toggle_dtr(self):
        self.assertEqual(esptool_reset_mode("darwin"), ("no_reset", "no_reset"))
        self.assertIn("cu.usbserial", port_list_hint("darwin"))


if __name__ == "__main__":
    unittest.main()
