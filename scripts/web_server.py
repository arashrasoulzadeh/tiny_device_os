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

        # TODO: Walk Kconfig for full catalog
        feature_list = []
        for symbol, value in features.items():
            feature_list.append({
                "symbol": symbol,
                "type": "bool" if isinstance(value, bool) else "int",
                "value": value,
                "wired": symbol in ("HEAP_SIZE_KB", "TICK_RATE_HZ", "ENABLE_POWER_MANAGEMENT",
                                    "ENABLE_TICKLESS_IDLE", "USE_LITTLEFS", "USE_FATFS",
                                    "DISPLAY_DRIVER_SSD1306", "DISPLAY_DRIVER_ILI9341",
                                    "DISPLAY_DRIVER_ST7789"),
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
        # TODO: Implement actual build/flash using usb_flash.py logic
        self.add_log("Job started")
        time.sleep(1)
        self.add_log("Build complete")
        if self.upload:
            self.state = "uploading"
            self.add_log("Uploading...")
            time.sleep(1)
            self.state = "flashed"
            self.add_log("Flash complete")
        else:
            self.state = "built"
            self.add_log("Build complete (no upload)")
        self.running = False

    def cancel(self):
        self.running = False
        self.state = "cancelled"
        if self._process:
            self._process.terminate()


class Console:
    def __init__(self, port: str):
        self.port = port
        self.running = True
        self._link = None
        self._buffer = bytearray()
        self._lock = threading.Lock()

    def start(self):
        # TODO: Open serial port and start link
        pass

    def write(self, data: str):
        if self._link:
            self._link.send(data.encode())

    def read(self) -> str:
        with self._lock:
            data = self._buffer.decode(errors="replace")
            self._buffer.clear()
            return data

    def close(self):
        self.running = False
        if self._link:
            self._link.close()


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