#!/usr/bin/env python3
"""Unit tests for scripts/device_config.py (device_config.yaml loader)."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from device_config import (  # noqa: E402
    KNOWN_TARGETS,
    generate_header,
    load_device_config,
    parse_simple_yaml,
    pin_conflict_warnings,
    resolve_pin,
)
from usb_flash import pio_env_for_target  # noqa: E402


SAMPLE = """
device:
  name: nodemcu
  arch: esp8266
  board: nodemcu
  pio_env: nodemcu
  port: auto
  baud: 115200

lcd:
  type: ssd1306
  width: 128
  height: 32
  bus: i2c
  address: 0x3C
  scl: D1
  sda: D2

inputs:
  buttons:
    - name: up
      pin: D5
      active_low: true
      pull: up
    - name: select
      pin: D6
      active_low: true
      pull: up
"""


class TestDeviceConfig(unittest.TestCase):
    def test_parse_nested_and_list(self):
        cfg = parse_simple_yaml(SAMPLE)
        self.assertEqual(cfg["device"]["arch"], "esp8266")
        self.assertEqual(cfg["lcd"]["width"], 128)
        self.assertEqual(cfg["lcd"]["address"], 0x3C)
        self.assertEqual(cfg["inputs"]["buttons"][0]["pin"], "D5")

    def test_nodemcu_pin_map(self):
        self.assertEqual(resolve_pin("D5", "nodemcu"), 14)
        self.assertEqual(resolve_pin("D6", "nodemcu"), 12)
        self.assertEqual(resolve_pin("D7", "nodemcu"), 13)
        self.assertEqual(resolve_pin("GPIO12"), 12)

    def test_load_repo_device_config(self):
        path = ROOT / "device_config.yaml"
        self.assertTrue(path.is_file(), "device_config.yaml must exist at repo root")
        cfg = load_device_config(path)
        self.assertEqual(cfg["device"]["name"], "nodemcu")
        self.assertEqual(cfg["device"]["pio_env"], "nodemcu")
        self.assertEqual(cfg["lcd"]["width"], 128)
        self.assertEqual(cfg["lcd"]["height"], 32)
        self.assertEqual(cfg["lcd"]["scl"], "D1")
        self.assertEqual(cfg["lcd"]["sda"], "D2")
        self.assertEqual(cfg["inputs"]["buttons"][0]["name"], "up")
        self.assertEqual(cfg["inputs"]["buttons"][0]["pin"], "D5")
        self.assertEqual(cfg["inputs"]["buttons"][1]["name"], "select")
        self.assertEqual(cfg["inputs"]["buttons"][1]["pin"], "D6")

    def test_header_defines_lcd_and_button(self):
        cfg = parse_simple_yaml(SAMPLE)
        header = generate_header(cfg)
        self.assertIn("#define ARDUBOT_LCD_WIDTH 128", header)
        self.assertIn("#define ARDUBOT_LCD_HEIGHT 32", header)
        self.assertIn("#define ARDUBOT_LCD_SCL_GPIO 5", header)
        self.assertIn("#define ARDUBOT_LCD_SDA_GPIO 4", header)
        self.assertIn("#define ARDUBOT_BTN_UP_GPIO 14", header)
        self.assertIn("#define ARDUBOT_BTN_SELECT_GPIO 12", header)
        self.assertIn("#define ARDUBOT_BUTTON_COUNT 2", header)

    def test_button_on_lcd_scl_warns(self):
        bad = SAMPLE.replace("pin: D5", "pin: D1")
        cfg = parse_simple_yaml(bad)
        warnings = pin_conflict_warnings(cfg)
        self.assertTrue(any("conflict" in w for w in warnings))

    def test_pio_env_mapping(self):
        cfg = parse_simple_yaml(SAMPLE)
        nodemcu = next(t for t in KNOWN_TARGETS if t["id"] == "nodemcu")
        self.assertEqual(pio_env_for_target(nodemcu, cfg), "nodemcu")
        esp32 = next(t for t in KNOWN_TARGETS if t["id"] == "esp32")
        self.assertEqual(pio_env_for_target(esp32, {"device": {}}), "esp32dev")

    def test_platformio_ini_exists(self):
        ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("[env:nodemcu]", ini)
        self.assertIn("espressif8266", ini)
        self.assertNotIn("\nlib_deps", ini)
        self.assertNotIn("\n\tlib_deps", ini)
        self.assertTrue((ROOT / "boards/nodemcu/src/main.cpp").is_file())
        self.assertTrue((ROOT / "boards/nodemcu/src/ssd1306_mini.h").is_file())
        self.assertIn("icons.c", ini)

    def test_load_from_temp_file(self):
        with tempfile.NamedTemporaryFile("w", suffix=".yaml", delete=False) as fh:
            fh.write(SAMPLE)
            path = fh.name
        try:
            cfg = load_device_config(path)
            self.assertEqual(cfg["device"]["board"], "nodemcu")
        finally:
            Path(path).unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()
