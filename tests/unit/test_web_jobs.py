#!/usr/bin/env python3
"""Unit tests for job handling in web_server.py."""

from __future__ import annotations

import json
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from web_server import WebServer, Job  # noqa: E402


class TestWebJobs(unittest.TestCase):
    def setUp(self):
        self.server = None

    def tearDown(self):
        if self.server:
            self.server.stop()
            self.server = None

    def start_server(self, host="127.0.0.1", port=0):
        self.server = WebServer(host=host, port=port, config_path="device_config.yaml")
        self.server.start()
        time.sleep(0.2)

    def test_job_state_machine(self):
        """Test job state transitions."""
        job = Job(port="/dev/ttyUSB0", upload=True, config_path="device_config.yaml")
        
        # Initial state
        self.assertEqual(job.state, "building")
        self.assertTrue(job.running)
        
        # Test to_dict
        d = job.to_dict()
        self.assertEqual(d["state"], "building")
        self.assertEqual(d["port"], "/dev/ttyUSB0")
        self.assertTrue(d["upload"])
        
        # Test cancel
        job.cancel()
        self.assertFalse(job.running)
        self.assertEqual(job.state, "cancelled")

    def test_job_log_buffer(self):
        """Test log line buffering."""
        job = Job(port=None, upload=False, config_path="device_config.yaml")
        
        job.add_log("line 1")
        job.add_log("line 2")
        
        lines = job.new_log_lines()
        self.assertEqual(lines, ["line 1", "line 2"])
        
        # Buffer should be cleared
        lines = job.new_log_lines()
        self.assertEqual(lines, [])

    def test_job_history(self):
        """Test job history is saved."""
        self.start_server()
        web_server = self.server
        
        # Clear existing history
        web_server.job_history = []
        web_server._save_job_history()
        
        # Check history file path
        self.assertTrue(str(web_server._history_file).endswith("build/web/jobs.json"))
        
        # Add a job to history
        job = Job(port="/dev/ttyUSB0", upload=True, config_path="device_config.yaml")
        job.state = "flashed"
        job.firmware_path = ".pio/build/nodemcu/firmware.bin"
        web_server._add_job_to_history(job)
        
        # Check history was saved
        history = json.loads(web_server._history_file.read_text())
        self.assertEqual(len(history), 1)
        self.assertEqual(history[0]["state"], "flashed")
        self.assertEqual(history[0]["port"], "/dev/ttyUSB0")

    def test_job_history_limit(self):
        """Test job history keeps only last 50 entries."""
        self.start_server()
        web_server = self.server
        
        # Clear existing history
        web_server.job_history = []
        web_server._save_job_history()
        
        # Add 55 jobs
        for i in range(55):
            job = Job(port=f"/dev/ttyUSB{i}", upload=True, config_path="device_config.yaml")
            job.state = "flashed"
            web_server._add_job_to_history(job)
        
        history = json.loads(web_server._history_file.read_text())
        self.assertEqual(len(history), 50)
        # Should keep the last 50 (indices 5-54)
        self.assertEqual(history[0]["port"], "/dev/ttyUSB5")
        self.assertEqual(history[-1]["port"], "/dev/ttyUSB54")

    def test_secret_redaction_in_logs(self):
        """Test that secrets are redacted in job logs."""
        from web_server import redact_secrets
        
        # Test password redaction
        text = 'wifi_password = "mysecretpassword"'
        result = redact_secrets(text)
        self.assertIn("***REDACTED***", result)
        self.assertNotIn("mysecretpassword", result)
        
        # Test link key redaction (matches link_key pattern first)
        text = 'link_key = "0123456789abcdef0123456789abcdef"'
        result = redact_secrets(text)
        self.assertIn("***REDACTED***", result)
        self.assertNotIn("0123456789abcdef", result)
        
        # Test no false positives
        text = 'building kernel...'
        result = redact_secrets(text)
        self.assertEqual(result, text)

    def test_concurrent_job_rejection(self):
        """Test that starting a second job while one is running returns error."""
        self.start_server()
        
        import urllib.request
        
        # Start first job
        data = json.dumps({"upload": False}).encode()
        req = urllib.request.Request(
            f"http://127.0.0.1:{self.server.port}/api/jobs",
            data=data,
            method="POST",
            headers={"Content-Type": "application/json"}
        )
        
        with urllib.request.urlopen(req, timeout=5) as resp:
            self.assertEqual(resp.status, 200)
        
        # Try to start second job - should fail with 409
        req2 = urllib.request.Request(
            f"http://127.0.0.1:{self.server.port}/api/jobs",
            data=data,
            method="POST",
            headers={"Content-Type": "application/json"}
        )
        
        try:
            with urllib.request.urlopen(req2, timeout=5) as resp:
                self.fail("Expected 409 error")
        except urllib.error.HTTPError as e:
            self.assertEqual(e.code, 409)


if __name__ == "__main__":
    unittest.main()