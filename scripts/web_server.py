#!/usr/bin/env python3
"""HTTP server for the ArdubotOS web flash console.

Serves static files from tools/web/ and a JSON/SSE API for device management,
build/flash jobs, and TTY console.
"""

from __future__ import annotations

import argparse
import http.server
import json
import mimetypes
import os
import signal
import socket
import sys
import threading
import time
import webbrowser
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import re


# Secret patterns for redaction
SECRET_PATTERNS = [
    (re.compile(r'(password|passwd|pwd)["\s:=]+[\w!@#$%^&*]+', re.IGNORECASE), r'\1=***REDACTED***'),
    (re.compile(r'(ssid)["\s:=]+[\w!@#$%^&*]+', re.IGNORECASE), r'\1=***REDACTED***'),
    (re.compile(r'(link\.key|link_key)["\s:=]+[0-9a-fA-F]+', re.IGNORECASE), r'\1=***REDACTED***'),
    (re.compile(r'(api[_-]?key|apikey)["\s:=]+[\w\-]+', re.IGNORECASE), r'\1=***REDACTED***'),
    (re.compile(r'[0-9a-fA-F]{32}'), '***REDACTED_KEY***'),
    (re.compile(r'[0-9a-fA-F]{64}'), '***REDACTED_KEY***'),
]

def redact_secrets(text: str) -> str:
    """Redact secrets from log text."""
    result = text
    for pattern, replacement in SECRET_PATTERNS:
        result = pattern.sub(replacement, result)
    return result

def parse_kconfig(kconfig_path: Path) -> list[dict]:
    """Parse Kconfig file and return list of symbol definitions."""
    if not kconfig_path.exists():
        return []
    
    content = kconfig_path.read_text(encoding="utf-8")
    symbols = []
    
    # Simple regex-based parsing for Kconfig
    lines = content.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        
        # Match config SYMBOL
        match = re.match(r'^config\s+(\w+)', line)
        if match:
            symbol = match.group(1)
            symbol_info = {"symbol": symbol, "type": "bool", "prompt": "", "default": "", "range": ""}
            i += 1
            
            # Parse symbol properties
            while i < len(lines):
                prop_line = lines[i].strip()
                if prop_line.startswith(("config ", "menu ", "choice ", "endchoice", "endmenu", "comment ")):
                    break
                
                if prop_line.startswith("bool") or prop_line.startswith("tristate"):
                    symbol_info["type"] = "bool"
                    # Extract prompt if present
                    prompt_match = re.match(r'bool\s+"([^"]+)"', prop_line)
                    if prompt_match:
                        symbol_info["prompt"] = prompt_match.group(1)
                elif prop_line.startswith("int ") or prop_line.startswith("hex "):
                    symbol_info["type"] = "int"
                    prompt_match = re.match(r'(int|hex)\s+"([^"]+)"', prop_line)
                    if prompt_match:
                        symbol_info["prompt"] = prompt_match.group(1)
                elif prop_line.startswith("string "):
                    symbol_info["type"] = "string"
                    prompt_match = re.match(r'string\s+"([^"]+)"', prop_line)
                    if prompt_match:
                        symbol_info["prompt"] = prompt_match.group(1)
                elif prop_line.startswith("default "):
                    # Extract default value
                    default_match = re.match(r'default\s+(\S+)', prop_line)
                    if default_match:
                        symbol_info["default"] = default_match.group(1)
                elif prop_line.startswith("range "):
                    symbol_info["range"] = prop_line[6:].strip()
                
                i += 1
            
            symbols.append(symbol_info)
            continue
        
        i += 1
    
    return symbols


from device_config import (  # noqa: E402
    list_serial_ports,
    load_device_config,
    pin_conflict_warnings,
    resolve_port,
)
from device_secrets import load_wifi_secrets  # noqa: E402

VERSION = "0.1.0"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8765
WEB_DIR = ROOT / "tools" / "web"


class APIHandler(http.server.BaseHTTPRequestHandler):
    server: "WebServer"

    def do_GET(self):
        if self.path == "/api/health":
            self.handle_health()
        elif self.path == "/api/ports":
            self.handle_ports()
        elif self.path == "/api/devices":
            self.handle_devices()
        elif self.path == "/api/apps":
            self.handle_apps()
        elif self.path == "/api/main-app":
            self.handle_main_app()
        elif self.path == "/api/features":
            self.handle_features()
        elif self.path == "/api/size":
            self.handle_size()
        elif self.path == "/api/jobs/current":
            self.handle_job_status()
        elif self.path == "/api/jobs/current/log":
            self.handle_job_log()
        elif self.path == "/api/console/output":
            self.handle_console_output()
        else:
            self.serve_static()

    def do_POST(self):
        if self.path == "/api/device/port":
            self.handle_device_port()
        elif self.path == "/api/apps":
            self.handle_apps_post()
        elif self.path == "/api/main-app":
            self.handle_main_app_post()
        elif self.path == "/api/features":
            self.handle_features_post()
        elif self.path == "/api/jobs":
            self.handle_jobs_post()
        elif self.path == "/api/jobs/current/cancel":
            self.handle_job_cancel()
        elif self.path == "/api/console":
            self.handle_console_open()
        elif self.path == "/api/console/input":
            self.handle_console_input()
        elif self.path == "/api/console/close":
            self.handle_console_close()
        else:
            self.send_error(404)

    def send_json(self, data: Any, status: int = 200):
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_sse(self, data: str):
        self.wfile.write(f"data: {data}\n\n".encode())

    def handle_health(self):
        from usb_flash import find_platformio, check_xtensa_toolchain_arch, host_is_apple_silicon, rosetta_available

        platformio = find_platformio()
        toolchain_err = check_xtensa_toolchain_arch()
        is_apple_silicon = host_is_apple_silicon()
        rosetta_ok = rosetta_available() if is_apple_silicon else True

        self.send_json({
            "version": VERSION,
            "repo_root": str(ROOT),
            "platformio_found": platformio is not None,
            "platformio_path": platformio[0] if platformio else None,
            "apple_silicon": is_apple_silicon,
            "rosetta_available": rosetta_ok,
            "xtensa_toolchain_ok": toolchain_err is None,
            "xtensa_toolchain_error": toolchain_err,
        })

    def handle_ports(self):
        ports = list_serial_ports()
        self.send_json(ports)

    def handle_devices(self):
        web_server = self.server.web_server
        config_path = web_server.config_path
        try:
            cfg = load_device_config(config_path)
        except Exception as e:
            self.send_json({"error": str(e)}, 500)
            return

        device = cfg.get("device") or {}
        port = resolve_port(cfg, web_server.session_port)
        port_status = "connected"
        if port is None:
            port_status = "missing"
        elif web_server.session_port:
            port_status = "overridden"

        warnings = pin_conflict_warnings(cfg)
        ssid, _ = load_wifi_secrets(ROOT / "device_secrets.yaml")
        secrets_present = bool(ssid)

        self.send_json({
            "config": cfg,
            "port": port,
            "port_status": port_status,
            "pin_warnings": warnings,
            "secrets_present": secrets_present,
        })

    def handle_apps(self):
        from device_config import load_stdapp_packages, resolve_apps

        cfg = load_device_config(self.server.web_server.config_path)
        pkgs = load_stdapp_packages()
        enabled, excluded = resolve_apps(cfg)

        apps = []
        for name, meta in pkgs.items():
            apps.append({
                "name": name,
                "title": meta.get("title", name.upper()),
                "description": meta.get("description", ""),
                "version": meta.get("version", "0.0.0"),
                "depends": meta.get("depends", []),
                "min_display": meta.get("min_display", {"width": 0, "height": 0}),
                "enabled": name in enabled,
                "exclude_reason": next((r for n, r in excluded if n == name), None),
            })

        self.send_json({"apps": apps, "enabled": enabled})

    def handle_apps_post(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode()
        data = json.loads(body) if body else {}
        apps = data.get("apps", [])

        self.server.web_server.write_apps_to_config(apps)
        self.send_json({"ok": True, "apps": apps})

    def handle_main_app(self):
        cfg = load_device_config(self.server.web_server.config_path)
        main_app = cfg.get("main_app", "launcher")
        self.send_json({"main_app": main_app})

    def handle_main_app_post(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode()
        data = json.loads(body) if body else {}
        main_app = data.get("main_app", "launcher")

        self.server.web_server.write_main_app_to_config(main_app)
        self.send_json({"ok": True, "main_app": main_app})

    def handle_features(self):
        from device_config import load_device_config

        cfg = load_device_config(self.server.web_server.config_path)
        features = cfg.get("features", {})

        # Walk Kconfig for full catalog
        kconfig_path = ROOT / "Kconfig"
        kconfig_symbols = parse_kconfig(kconfig_path)
        
        # Symbols that are known to be wired (actually affect compilation)
        wired_symbols = {
            "HEAP_SIZE_KB", "TICK_RATE_HZ", "ENABLE_POWER_MANAGEMENT",
            "ENABLE_TICKLESS_IDLE", "USE_LITTLEFS", "USE_FATFS",
            "DISPLAY_DRIVER_SSD1306", "DISPLAY_DRIVER_ILI9341",
            "DISPLAY_DRIVER_ST7789", "MAX_TASKS", "STACK_GUARD_SIZE",
            "DEEP_SLEEP_MIN_US", "USE_TLSF_ALLOCATOR", "LITTLEFS_BLOCK_SIZE",
            "LITTLEFS_LOOKAHEAD", "VIRTUAL_CANVAS_WIDTH", "VIRTUAL_CANVAS_HEIGHT",
            "UI_FRAMEWORK", "UI_THEMES", "UI_ANIMATIONS",
            "HELPER_STRINGS", "HELPER_MATH", "HELPER_COLLECTIONS",
            "HELPER_DEBUG", "HELPER_PROFILING", "HELPER_CONFIG",
            "HELPER_TIME", "HELPER_FS",
        }
        
        # Also include architecture-specific symbols
        device = cfg.get("device") or {}
        arch = device.get("arch", "esp8266")
        if arch == "sim":
            wired_symbols.update({"SIM_HEADLESS", "SIM_AUDIO", "SIM_NETWORK"})
        
        feature_list = []
        for symbol_info in kconfig_symbols:
            symbol = symbol_info["symbol"]
            # Skip architecture selection symbols
            if symbol.startswith("ARDUINO_ARCH_"):
                continue
            
            current_value = features.get(symbol, symbol_info.get("default", ""))
            
            # Convert default to appropriate type
            if symbol_info["type"] == "bool":
                if current_value in ("y", "Y", "1", True, "true"):
                    current_value = True
                elif current_value in ("n", "N", "0", False, "false", ""):
                    current_value = False
            elif symbol_info["type"] == "int":
                try:
                    current_value = int(current_value)
                except (ValueError, TypeError):
                    current_value = 0
            
            is_wired = symbol in wired_symbols
            
            feature_list.append({
                "symbol": symbol,
                "type": symbol_info["type"],
                "prompt": symbol_info["prompt"],
                "default": symbol_info["default"],
                "range": symbol_info["range"],
                "value": current_value,
                "wired": is_wired,
            })

        self.send_json({"features": feature_list})

    def handle_features_post(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode()
        data = json.loads(body) if body else {}
        features = data.get("features", {})

        self.server.web_server.write_features_to_config(features)
        self.send_json({"ok": True, "features": features})

    def handle_size(self):
        size_file = ROOT / "build" / "web" / "last_size.json"
        if size_file.exists():
            data = json.loads(size_file.read_text())
            # Check if stale (apps/features changed since last build)
            # For now just return the data
            self.send_json(data)
        else:
            cfg = load_device_config(self.server.web_server.config_path)
            flash_size = cfg.get("device", {}).get("flash_size", "4MB")
            self.send_json({
                "budget_flash": flash_size,
                "measured": False,
                "message": "no measured size yet",
            })

    def handle_job_status(self):
        job = self.server.web_server.current_job
        if job:
            self.send_json(job.to_dict())
        else:
            self.send_json({"state": "idle"})

    def handle_job_log(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        job = self.server.web_server.current_job
        if job:
            for line in job.log_lines:
                self.send_sse(json.dumps({"line": line}))
            # Keep connection open for new lines
            while job.running:
                time.sleep(0.5)
                for line in job.new_log_lines():
                    self.send_sse(json.dumps({"line": line}))

    def handle_console_output(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        console = self.server.web_server.console
        if console:
            while console.running:
                time.sleep(0.1)
                data = console.read()
                if data:
                    self.send_sse(json.dumps({"data": data}))

    def handle_device_port(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode()
        data = json.loads(body) if body else {}
        port = data.get("port")

        if port and not Path(port).exists():
            self.send_json({"error": "port does not exist"}, 400)
            return

        self.server.web_server.session_port = port
        self.send_json({"ok": True, "port": port})

    def handle_jobs_post(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode()
        data = json.loads(body) if body else {}

        if self.server.web_server.current_job and self.server.web_server.current_job.running:
            self.send_json({"error": "job already running"}, 409)
            return

        upload = data.get("upload", True)
        port = self.server.web_server.session_port or resolve_port(
            load_device_config(self.server.web_server.config_path), None
        )

        job = self.server.web_server.start_job(port, upload)
        self.send_json(job.to_dict())

    def handle_job_cancel(self):
        job = self.server.web_server.current_job
        if job:
            job.cancel()
            self.send_json({"ok": True})
        else:
            self.send_json({"error": "no job running"}, 404)

    def handle_console_open(self):
        if self.server.web_server.current_job and self.server.web_server.current_job.running:
            self.send_json({"error": "flash job holds port"}, 409)
            return

        port = self.server.web_server.session_port or resolve_port(
            load_device_config(self.server.web_server.config_path), None
        )
        if not port or not Path(port).exists():
            self.send_json({"error": "no serial port"}, 400)
            return

        console = self.server.web_server.start_console(port)
        self.send_json({"ok": True, "port": port})

    def handle_console_input(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode()
        data = json.loads(body) if body else {}
        input_data = data.get("data", "")

        console = self.server.web_server.console
        if console:
            console.write(input_data)
            self.send_json({"ok": True})
        else:
            self.send_json({"error": "console not open"}, 404)

    def handle_console_close(self):
        if self.server.web_server.console:
            self.server.web_server.console.close()
            self.server.web_server.console = None
        self.send_json({"ok": True})

    def serve_static(self):
        path = self.path.lstrip("/")
        if not path:
            path = "index.html"

        file_path = WEB_DIR / path
        if file_path.exists() and file_path.is_file():
            mime_type, _ = mimetypes.guess_type(str(file_path))
            self.send_response(200)
            self.send_header("Content-Type", mime_type or "application/octet-stream")
            self.end_headers()
            self.wfile.write(file_path.read_bytes())
        else:
            index_file = WEB_DIR / "index.html"
            if index_file.exists():
                self.send_response(200)
                self.send_header("Content-Type", "text/html")
                self.end_headers()
                self.wfile.write(index_file.read_bytes())
            else:
                self.send_error(404)

    def log_message(self, format, *args):
        pass


class WebServer:
    def __init__(self, host: str, port: int, config_path: str):
        if host != "127.0.0.1" and host != "localhost":
            print("error: --host must be 127.0.0.1 (loopback only in v1)", file=sys.stderr)
            sys.exit(2)

        self.host = host
        self.port = port
        self.config_path = config_path
        self.session_port: str | None = None
        self.current_job: "Job | None" = None
        self.console: "Console | None" = None
        self.running = False
        self._server: http.server.HTTPServer | None = None
        self._thread: threading.Thread | None = None
        self.job_history: list[dict] = []
        self._history_file = ROOT / "build" / "web" / "jobs.json"
        self._load_job_history()

    def _load_job_history(self):
        if self._history_file.exists():
            try:
                self.job_history = json.loads(self._history_file.read_text())
            except Exception:
                self.job_history = []

    def _save_job_history(self):
        self._history_file.parent.mkdir(parents=True, exist_ok=True)
        # Keep last 50 jobs
        self.job_history = self.job_history[-50:]
        self._history_file.write_text(json.dumps(self.job_history, indent=2))

    def _add_job_to_history(self, job: "Job"):
        entry = job.to_dict()
        entry["timestamp"] = time.time()
        entry["config_path"] = self.config_path
        self.job_history.append(entry)
        self._save_job_history()

    def start(self):
        self._server = http.server.HTTPServer((self.host, self.port), APIHandler)
        self._server.web_server = self  # Attach WebServer instance
        self.port = self._server.server_port
        self.running = True
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()

    def stop(self):
        self.running = False
        if self._server:
            self._server.shutdown()
            self._server.server_close()
        if self._thread:
            self._thread.join(timeout=1.0)

    import tempfile

    def write_apps_to_config(self, apps: list[str]):
        self._update_yaml_field("apps", apps)

    def write_main_app_to_config(self, main_app: str):
        self._update_yaml_field("main_app", main_app)

    def write_features_to_config(self, features: dict):
        self._update_yaml_field("features", features)

    def _update_yaml_field(self, field: str, value: Any):
        """Surgically update a field in device_config.yaml, preserving comments and formatting."""
        import tempfile
        import os
        config_path = Path(self.config_path)
        if not config_path.exists():
            raise FileNotFoundError(f"{config_path} does not exist")

        # Read original content
        original = config_path.read_text(encoding="utf-8")
        
        # Create backup
        backup_path = config_path.with_suffix(config_path.suffix + ".bak")
        if not backup_path.exists():
            backup_path.write_text(original, encoding="utf-8")

        # Parse to validate the new value
        from device_config import parse_simple_yaml
        test_cfg = parse_simple_yaml(original)
        test_cfg[field] = value
        # Re-serialize to validate
        # We just need to ensure the value is valid YAML for our parser

        # Find and replace the field
        lines = original.splitlines(keepends=True)
        new_lines = []
        i = 0
        in_field = False
        field_indent = None
        
        while i < len(lines):
            line = lines[i]
            stripped = line.lstrip()
            
            # Check if this line starts the field
            if not in_field and stripped.startswith(f"{field}:"):
                # Found the field
                indent = len(line) - len(stripped)
                field_indent = indent
                in_field = True
                
                # Write the new field
                if isinstance(value, list):
                    # List of scalars
                    new_lines.append(f"{' ' * indent}{field}:\n")
                    for item in value:
                        new_lines.append(f"{' ' * (indent + 2)}- {item}\n")
                elif isinstance(value, dict):
                    # Map of scalars
                    new_lines.append(f"{' ' * indent}{field}:\n")
                    for k, v in value.items():
                        if isinstance(v, bool):
                            v_str = "true" if v else "false"
                        elif isinstance(v, int):
                            v_str = str(v)
                        else:
                            v_str = str(v)
                        new_lines.append(f"{' ' * (indent + 2)}{k}: {v_str}\n")
                else:
                    # Scalar value
                    if isinstance(value, bool):
                        v_str = "true" if value else "false"
                    elif isinstance(value, int):
                        v_str = str(value)
                    else:
                        v_str = str(value)
                    new_lines.append(f"{' ' * indent}{field}: {v_str}\n")
                
                # Skip the old field lines
                i += 1
                while i < len(lines):
                    next_line = lines[i]
                    next_stripped = next_line.lstrip()
                    next_indent = len(next_line) - len(next_stripped) if next_stripped else 0
                    
                    # Stop when we hit a line at same or less indent that's not a comment/empty
                    if next_stripped and not next_stripped.startswith("#") and next_indent <= field_indent:
                        break
                    # Also stop at end of file
                    i += 1
                continue
            
            new_lines.append(line)
            i += 1

        # If field was not found, add it at the end (before any trailing comments/empty lines)
        if not in_field:
            # Find a good insertion point - after the last top-level field
            # We'll insert before the last non-empty, non-comment line if it's at indent 0
            # Or just append at the end
            new_lines.append(f"\n{field}:\n")
            if isinstance(value, list):
                for item in value:
                    new_lines.append(f"  - {item}\n")
            elif isinstance(value, dict):
                for k, v in value.items():
                    if isinstance(v, bool):
                        v_str = "true" if v else "false"
                    elif isinstance(v, int):
                        v_str = str(v)
                    else:
                        v_str = str(v)
                    new_lines.append(f"  {k}: {v_str}\n")
            else:
                if isinstance(value, bool):
                    v_str = "true" if value else "false"
                elif isinstance(value, int):
                    v_str = str(value)
                else:
                    v_str = str(value)
                new_lines[-1] = f"{field}: {v_str}\n"

        # Write to temp file then atomically replace
        with tempfile.NamedTemporaryFile(mode='w', dir=config_path.parent, delete=False, suffix='.tmp') as tmp:
            tmp.write(''.join(new_lines))
            tmp_path = Path(tmp.name)
        
        try:
            os.replace(tmp_path, config_path)
        except Exception:
            tmp_path.unlink(missing_ok=True)
            raise

    def start_job(self, port: str | None, upload: bool) -> "Job":
        job = Job(port, upload, self.config_path)
        job._web_server_ref = self
        self.current_job = job
        job.start()
        return job

    def start_console(self, port: str) -> "Console":
        from link_codec import Link
        from link_monitor import tty_present, tty_local_action

        console = Console(port)
        self.console = console
        console.start()
        return console


class Job:
    def __init__(self, port: str | None, upload: bool, config_path: str):
        self.port = port
        self.upload = upload
        self.config_path = config_path
        self.state = "building"
        self.running = True
        self.log_lines: list[str] = []
        self._log_lock = threading.Lock()
        self._process = None
        self._thread: threading.Thread | None = None
        self.start_time = time.time()
        self.firmware_path: str | None = None
        self.error: str | None = None

    def to_dict(self):
        return {
            "state": self.state,
            "port": self.port,
            "upload": self.upload,
            "elapsed": time.time() - self.start_time,
            "firmware_path": self.firmware_path,
            "error": self.error,
        }

    def new_log_lines(self):
        with self._log_lock:
            lines = self.log_lines.copy()
            self.log_lines.clear()
            return lines

    def add_log(self, line: str):
        with self._log_lock:
            self.log_lines.append(line)

    def start(self):
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        import subprocess
        import os
        
        try:
            # Run the same steps as usb_flash.py
            from device_config import (
                load_device_config, pin_conflict_warnings, resolve_port, write_header
            )
            from device_secrets import write_secrets_header, SecretsError
            from usb_flash import (
                find_platformio, pio_env_for_target, run_platformio,
                check_xtensa_toolchain_arch, host_is_apple_silicon
            )
            
            cfg = load_device_config(self.config_path)
            
            # Pin warnings
            for w in pin_conflict_warnings(cfg):
                self.add_log(f"warning: {w}")
            
            # Determine target from config
            device = cfg.get("device") or {}
            target_id = device.get("board", "nodemcu")
            
            # Find target
            from device_config import KNOWN_TARGETS
            target = None
            for t in KNOWN_TARGETS:
                if t["id"] == target_id or t["board"] == target_id:
                    target = t
                    break
            
            if not target:
                self.error = f"Unknown target: {target_id}"
                self.state = "failed"
                self.running = False
                return
            
            arch = target["arch"]
            self.add_log(f"Target: {target['label']} (arch={arch})")
            
            # Generate device_config.h
            header_path = ROOT / "build" / "generated" / "device_config.h"
            cfg_for_header = dict(cfg)
            cfg_for_header["device"] = dict(cfg.get("device") or {})
            cfg_for_header["device"]["arch"] = arch
            cfg_for_header["device"]["board"] = target["board"]
            write_header(cfg_for_header, header_path)
            self.add_log(f"Generated {header_path.relative_to(ROOT)}")
            
            # Generate device_secrets.h
            secrets_header = ROOT / "build" / "generated" / "device_secrets.h"
            notes = cfg.get("notifications") or {}
            forward = isinstance(notes, dict) and bool(notes.get("forward_from_host"))
            try:
                write_secrets_header(
                    ROOT / "device_secrets.yaml",
                    secrets_header,
                    require=(arch != "sim"),
                    require_link=forward and arch != "sim",
                )
                self.add_log(f"Generated {secrets_header.relative_to(ROOT)}")
            except SecretsError as exc:
                self.error = str(exc)
                self.state = "failed"
                self.running = False
                return
            
            if arch == "sim":
                self.add_log("Simulator target selected - not flashing")
                self.state = "built"
                self.running = False
                return
            
            # Get PlatformIO env
            env_name = pio_env_for_target(target, cfg)
            if not env_name:
                self.error = f"No PlatformIO env mapped for {target['id']}"
                self.state = "failed"
                self.running = False
                return
            
            # Find PlatformIO
            platformio = find_platformio()
            if not platformio:
                self.error = "`platformio` not found on PATH"
                self.add_log(self.error)
                self.state = "failed"
                self.running = False
                return
            
            # Check xtensa toolchain on Apple Silicon
            if arch in ("esp8266",) and host_is_apple_silicon():
                toolchain_err = check_xtensa_toolchain_arch()
                if toolchain_err:
                    self.error = toolchain_err
                    self.add_log(self.error)
                    self.state = "failed"
                    self.running = False
                    return
                if not rosetta_available():
                    self.add_log("warning: ESP8266 toolchain is x86_64; Rosetta required on Apple Silicon.")
            
            # Resolve port
            port = resolve_port(cfg, self.port)
            if port and not os.path.exists(port):
                self.add_log(f"warning: configured port {port} is missing")
                port = None
            
            if not port:
                ports = list_serial_ports()
                if not ports:
                    self.add_log("error: no USB serial port found")
                    self.add_log("The CH340 device is not visible. Unplug, wait 2s, replug directly.")
                    if ports:
                        self.add_log(f"Detected: {', '.join(ports)}")
                    self.add_log("Continuing with build only (no upload)...")
                    upload = False
                else:
                    port = ports[0]
            
            self.port = port
            self.add_log(f"Serial port: {port or '(none - build only)'}")
            self.add_log(f"PlatformIO env: {env_name}")
            
            if arch == "esp8266" and port:
                self.add_log("Note: CH340 cannot use Arduino auto-reset. Uploader will ask to hold FLASH + tap RST.")
            
            # Run PlatformIO build
            self.add_log("Starting PlatformIO build...")
            self.state = "building"
            
            def log_callback(line):
                self.add_log(line.rstrip())
            
            # Run platformio with output capture
            cmd = [*platformio, "run", "-e", env_name, "-d", str(ROOT)]
            self.add_log(f"+ {' '.join(cmd)}")
            
            self._process = subprocess.Popen(
                cmd,
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
            )
            
            # Stream output
            if self._process.stdout:
                for line in self._process.stdout:
                    if not self.running:
                        self._process.terminate()
                        break
                    self.add_log(line.rstrip())
            
            rc = self._process.wait()
            self._process = None
            
            if rc != 0:
                self.error = f"Build failed with exit code {rc}"
                self.add_log(self.error)
                self.state = "failed"
                self.running = False
                return
            
            self.add_log("Build successful")
            
            # Find firmware path
            firmware_path = ROOT / ".pio" / "build" / env_name / "firmware.bin"
            if firmware_path.exists():
                self.firmware_path = str(firmware_path.relative_to(ROOT))
                self.add_log(f"Firmware: {self.firmware_path}")
            
            # Upload if requested and port available
            if self.upload and port:
                self.state = "uploading"
                self.add_log("Starting upload...")
                
                upload_cmd = [
                    *platformio, "run", "-e", env_name, "-d", str(ROOT),
                    "-t", "upload", "--upload-port", port
                ]
                self.add_log(f"+ {' '.join(upload_cmd)}")
                
                self._process = subprocess.Popen(
                    upload_cmd,
                    cwd=ROOT,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    bufsize=1,
                )
                
                if self._process.stdout:
                    for line in self._process.stdout:
                        if not self.running:
                            self._process.terminate()
                            break
                        self.add_log(line.rstrip())
                
                rc = self._process.wait()
                self._process = None
                
                if rc != 0:
                    self.error = f"Upload failed with exit code {rc}"
                    self.add_log(self.error)
                    self.state = "failed"
                    self.running = False
                    return
                
                self.add_log("Upload successful")
                self.state = "flashed"
            elif not self.upload:
                self.state = "built"
            elif not port:
                self.state = "built"
                self.add_log("Build complete (no port for upload)")
            
            # Generate and save size report on success
            if self.state in ("flashed", "built"):
                try:
                    from web_sizes import generate_size_report, save_size_report
                    map_path = ROOT / ".pio" / "build" / env_name / "firmware.map"
                    report = generate_size_report(map_path)
                    if report:
                        size_file = ROOT / "build" / "web" / "last_size.json"
                        save_size_report(report, size_file)
                        self.add_log(f"Size report saved to {size_file.relative_to(ROOT)}")
                except Exception as e:
                    self.add_log(f"Warning: Failed to generate size report: {e}")
            
        except Exception as e:
            self.error = f"Job error: {e}"
            self.add_log(self.error)
            self.state = "failed"
        finally:
            self.running = False
            # Add to job history
            web_server = getattr(self, '_web_server_ref', None)
            if web_server and hasattr(web_server, '_add_job_to_history'):
                web_server._add_job_to_history(self)

    def cancel(self):
        self.running = False
        self.state = "cancelled"
        if self._process:
            try:
                self._process.terminate()
            except Exception:
                pass


class Console:
    def __init__(self, port: str, baud: int = 115200):
        self.port = port
        self.baud = baud
        self.running = True
        self._link = None
        self._serial = None
        self._buffer = bytearray()
        self._lock = threading.Lock()
        self._read_thread: threading.Thread | None = None

    def start(self):
        import serial
        from device_secrets import load_link_secrets
        from link_codec import Link
        from link_monitor import tty_present, tty_local_action
        
        try:
            self._serial = serial.Serial(self.port, self.baud, timeout=0.1)
        except Exception as e:
            self.running = False
            raise RuntimeError(f"Failed to open serial port {self.port}: {e}")
        
        # Check for link key
        key_id, key = load_link_secrets(ROOT / "device_secrets.yaml")
        if key:
            self._link = Link(key, key_id)
            self._link.open_serial(self._serial)
            self.add_log("Link encryption enabled")
        else:
            self.add_log("No link key - using raw monitor")
        
        self._read_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._read_thread.start()

    def _read_loop(self):
        from link_monitor import tty_present
        
        while self.running and self._serial:
            try:
                if self._serial.in_waiting:
                    data = self._serial.read(self._serial.in_waiting)
                    if self._link:
                        # Decrypt through link
                        for byte in data:
                            result = self._link.receive(bytes([byte]))
                            if result:
                                with self._lock:
                                    self._buffer.extend(result)
                    else:
                        # Raw monitor - process through tty_present
                        processed = tty_present(data.decode(errors="replace"))
                        with self._lock:
                            self._buffer.extend(processed.encode())
            except Exception:
                pass
            time.sleep(0.01)

    def write(self, data: str):
        if not self._serial:
            return
        try:
            if self._link:
                self._link.send(data.encode())
            else:
                self._serial.write(data.encode())
        except Exception:
            pass

    def read(self) -> str:
        with self._lock:
            data = self._buffer.decode(errors="replace")
            self._buffer.clear()
            return data

    def add_log(self, msg: str):
        """Add a log message to the buffer."""
        with self._lock:
            self._buffer.extend(f"[console] {msg}\n".encode())

    def close(self):
        self.running = False
        if self._link:
            try:
                self._link.close()
            except Exception:
                pass
        if self._serial:
            try:
                self._serial.close()
            except Exception:
                pass
        if self._read_thread:
            self._read_thread.join(timeout=1.0)


def main():
    parser = argparse.ArgumentParser(description="ArdubotOS web flash console")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Bind address (loopback only)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Port to listen on")
    parser.add_argument("--config", default="device_config.yaml", help="Device config path")
    parser.add_argument("--open", action="store_true", default=None, help="Open browser")
    args = parser.parse_args()

    config_path = Path(args.config)
    if not config_path.is_absolute():
        config_path = ROOT / config_path

    server = WebServer(args.host, args.port, str(config_path))

    def signal_handler(sig, frame):
        server.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    server.start()
    print(f"Web server running at http://{server.host}:{server.port}")

    open_browser = args.open if args.open is not None else sys.stdout.isatty()
    if open_browser:
        try:
            webbrowser.open(f"http://{server.host}:{server.port}")
        except Exception:
            pass

    try:
        while server.running:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        server.stop()


if __name__ == "__main__":
    main()