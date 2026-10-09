#!/usr/bin/env python3
"""Unit tests for scripts/web_sizes.py."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from web_sizes import (  # noqa: E402
    parse_firmware_map,
    parse_platformio_size_output,
    generate_size_report,
    compute_delta,
)


class TestWebSizes(unittest.TestCase):
    def test_parse_platformio_size_output(self):
        output = """
RAM:   [==        ]  12.3% (used 12345 bytes from 100000 bytes)
Flash: [===       ]  23.4% (used 23456 bytes from 100000 bytes)
"""
        result = parse_platformio_size_output(output)
        self.assertIsNotNone(result)
        self.assertEqual(result["ram_used"], 12345)
        self.assertEqual(result["ram_total"], 100000)
        self.assertEqual(result["flash_used"], 23456)
        self.assertEqual(result["flash_total"], 100000)

    def test_parse_platformio_size_output_no_match(self):
        output = "Some other output"
        result = parse_platformio_size_output(output)
        self.assertIsNone(result)

    def test_compute_delta(self):
        prev = {
            "total_flash": 100000,
            "total_ram": 50000,
            "categories": {
                "kernel": {"flash": 30000, "ram": 10000},
                "apps": {
                    "counter": {"flash": 5000, "ram": 2000},
                    "clock": {"flash": 8000, "ram": 3000},
                },
                "runtime": {"flash": 20000, "ram": 15000},
            },
        }
        curr = {
            "total_flash": 110000,
            "total_ram": 52000,
            "categories": {
                "kernel": {"flash": 31000, "ram": 10500},
                "apps": {
                    "counter": {"flash": 5500, "ram": 2100},
                    "clock": {"flash": 8200, "ram": 3200},
                    "pong": {"flash": 6000, "ram": 2500},
                },
                "runtime": {"flash": 22000, "ram": 16000},
            },
        }
        delta = compute_delta(prev, curr)
        self.assertEqual(delta["total_flash"], 10000)
        self.assertEqual(delta["total_ram"], 2000)
        self.assertEqual(delta["categories"]["kernel"]["flash"], 1000)
        self.assertEqual(delta["categories"]["kernel"]["ram"], 500)
        self.assertEqual(delta["categories"]["apps"]["counter"]["flash"], 500)
        self.assertEqual(delta["categories"]["apps"]["pong"]["flash"], 6000)

    def test_parse_firmware_map_no_file(self):
        result = parse_firmware_map(Path("/nonexistent/map"))
        self.assertIsNone(result)

    def test_generate_size_report_no_file(self):
        result = generate_size_report(Path("/nonexistent/map"))
        self.assertIsNone(result)


if __name__ == "__main__":
    unittest.main()