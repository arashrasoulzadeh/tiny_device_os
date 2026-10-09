#!/usr/bin/env python3
"""Integration test for web_server.py - starts server on ephemeral port and tests API endpoints."""

from __future__ import annotations

import json
import sys
import time
import unittest
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from web_server import WebServer  # noqa: E402


class TestWebIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = WebServer(host="127.0.0.1", port=0, config_path="device_config.yaml")
        cls.server.start()
        time.sleep(0.5)
        cls.base_url = f"http://127.0.0.1:{cls.server.port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.stop()

    def _request(self, path: str, method: str = "GET", data: dict | None = None):
        url = f"{self.base_url}{path}"
        headers = {"Content-Type": "application/json"}
        if data:
            req = urllib.request.Request(
                url, data=json.dumps(data).encode(), method=method, headers=headers
            )
        else:
            req = urllib.request.Request(url, method=method)
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status, json.loads(resp.read().decode())

    def test_health_endpoint(self):
        status, data = self._request("/api/health")
        self.assertEqual(status, 200)
        self.assertIn("version", data)
        self.assertIn("repo_root", data)
        self.assertIn("platformio_found", data)

    def test_ports_endpoint(self):
        status, data = self._request("/api/ports")
        self.assertEqual(status, 200)
        self.assertIsInstance(data, list)

    def test_devices_endpoint(self):
        status, data = self._request("/api/devices")
        self.assertEqual(status, 200)
        self.assertIn("config", data)
        self.assertIn("port_status", data)
        self.assertIn("secrets_present", data)

    def test_apps_endpoint(self):
        status, data = self._request("/api/apps")
        self.assertEqual(status, 200)
        self.assertIn("apps", data)
        self.assertIn("enabled", data)

    def test_features_endpoint(self):
        status, data = self._request("/api/features")
        self.assertEqual(status, 200)
        self.assertIn("features", data)
        self.assertIsInstance(data["features"], list)

    def test_size_endpoint(self):
        status, data = self._request("/api/size")
        self.assertEqual(status, 200)
        self.assertIn("budget_flash", data)
        self.assertIn("measured", data)

    def test_job_status_idle(self):
        status, data = self._request("/api/jobs/current")
        self.assertEqual(status, 200)
        self.assertEqual(data["state"], "idle")


if __name__ == "__main__":
    unittest.main()