#!/usr/bin/env python3
"""Unit tests for scripts/web_server.py."""

from __future__ import annotations

import json
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from web_server import WebServer  # noqa: E402


class TestWebServer(unittest.TestCase):
    def setUp(self):
        self.server = None
        self.port = 0
        self.base_url = ""

    def tearDown(self):
        if self.server:
            self.server.stop()
            self.server = None

    def start_server(self, host="127.0.0.1", port=0):
        self.server = WebServer(host=host, port=port, config_path="device_config.yaml")
        self.server.start()
        # Wait for server to be ready
        time.sleep(0.2)
        self.port = self.server.port
        self.base_url = f"http://{host}:{self.port}"

    def test_server_binds_and_health_endpoint(self):
        self.start_server()
        import urllib.request

        with urllib.request.urlopen(f"{self.base_url}/api/health") as resp:
            self.assertEqual(resp.status, 200)
            data = json.loads(resp.read().decode())
            self.assertIn("version", data)
            self.assertIn("repo_root", data)
            self.assertIn("platformio_found", data)

    def test_second_bind_fails(self):
        self.start_server()
        # Try to create another server on the same port
        server2 = WebServer(host="127.0.0.1", port=self.port, config_path="device_config.yaml")
        with self.assertRaises(OSError):
            server2.start()
        server2.stop()

    def test_server_exits_on_sigint(self):
        self.start_server()
        import signal
        import os

        # Send SIGINT to the server thread
        self.server.stop()
        time.sleep(0.1)
        self.assertFalse(self.server.running)

    def test_refuses_non_loopback_host(self):
        with self.assertRaises(SystemExit) as cm:
            WebServer(host="0.0.0.0", port=0, config_path="device_config.yaml")
        self.assertEqual(cm.exception.code, 2)

    def test_serves_static_files(self):
        self.start_server()
        import urllib.request

        # Create a test static file
        web_dir = ROOT / "tools" / "web"
        web_dir.mkdir(parents=True, exist_ok=True)
        test_file = web_dir / "test.txt"
        test_file.write_text("hello world")

        try:
            with urllib.request.urlopen(f"{self.base_url}/test.txt") as resp:
                self.assertEqual(resp.status, 200)
                self.assertEqual(resp.read().decode(), "hello world")
        finally:
            test_file.unlink(missing_ok=True)

    def test_ports_endpoint(self):
        self.start_server()
        import urllib.request

        with urllib.request.urlopen(f"{self.base_url}/api/ports") as resp:
            self.assertEqual(resp.status, 200)
            data = json.loads(resp.read().decode())
            self.assertIsInstance(data, list)

    def test_devices_endpoint(self):
        self.start_server()
        import urllib.request

        with urllib.request.urlopen(f"{self.base_url}/api/devices") as resp:
            self.assertEqual(resp.status, 200)
            data = json.loads(resp.read().decode())
            self.assertIn("config", data)
            self.assertIn("port_status", data)


if __name__ == "__main__":
    unittest.main()