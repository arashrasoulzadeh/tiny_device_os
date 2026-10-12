#!/usr/bin/env python3
"""Linux serial-port names and esptool reset policy for the NodeMCU uploader."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from nodemcu_upload import (  # noqa: E402
    PORT_GLOBS,
    esptool_reset_mode,
    port_list_hint,
    rank_ports,
    reset_attempts,
)


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

    def test_linux_asks_for_flash_reset_when_auto_reset_fails(self):
        self.assertEqual(
            reset_attempts("linux"),
            [("default_reset", "hard_reset"), ("no_reset", "no_reset")],
        )
        self.assertEqual(reset_attempts("darwin"), [("no_reset", "no_reset")])

    def test_ch340_uart_is_chosen_over_native_usb(self):
        ports = ["/dev/ttyACM0", "/dev/ttyUSB0"]
        self.assertEqual(rank_ports(ports, None), ["/dev/ttyUSB0", "/dev/ttyACM0"])
        self.assertEqual(
            rank_ports(ports, "/dev/ttyACM0"), ["/dev/ttyUSB0", "/dev/ttyACM0"]
        )

    def test_explicit_uart_port_stays_first(self):
        ports = ["/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyACM0"]
        self.assertEqual(
            rank_ports(ports, "/dev/ttyUSB1"),
            ["/dev/ttyUSB1", "/dev/ttyUSB0", "/dev/ttyACM0"],
        )


if __name__ == "__main__":
    unittest.main()
