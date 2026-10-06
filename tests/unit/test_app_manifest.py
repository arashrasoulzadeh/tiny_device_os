#!/usr/bin/env python3
"""Unit tests for scripts/app_manifest.py."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from app_manifest import load_apps, render_inc, write_manifest  # noqa: E402


class AppManifestTests(unittest.TestCase):
    def test_render_escapes_quotes_and_fills_missing_fields(self):
        text = render_inc(
            [
                ("ping", {"description": 'say "hi"'}),
                ("pong", {"version": "3", "author": "Ada", "description": "ball"}),
            ]
        )
        self.assertIn('{ "ping", "1.0.0", "ArdubotOS", "say \\"hi\\"" },', text)
        self.assertIn('{ "pong", "3", "Ada", "ball" },', text)

    def test_load_apps_reads_app_json_by_directory_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            app = base / "temp"
            app.mkdir()
            (app / "app.json").write_text(json.dumps({"name": "temp", "version": "9"}))
            (base / "empty").mkdir()
            loaded = load_apps(base)
            self.assertEqual([("temp", {"name": "temp", "version": "9"})], loaded)
            out = base / "out.inc"
            write_manifest(base, out)
            self.assertIn('"temp"', out.read_text())
            self.assertIn('"9"', out.read_text())


if __name__ == "__main__":
    unittest.main()
