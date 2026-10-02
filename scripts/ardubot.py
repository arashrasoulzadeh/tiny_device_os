#!/usr/bin/env python3
"""ardubot - single entry point CLI for ArdubotOS.

    ardubot flash               build + flash the current device_config.yaml target
    ardubot monitor             open a serial monitor on the device's port
    ardubot create-app <name>   scaffold a new stdapp from the app_ui kit template

Thin wrapper over the existing Makefile / scripts/*.py tooling - it exists so
there is one documented command instead of five different `make` incantations.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))

from device_config import load_device_config, resolve_port  # noqa: E402


def cmd_flash(args: argparse.Namespace) -> int:
    cmd = ["python3", "scripts/usb_flash.py", "--config", args.config]
    if args.device:
        cmd += ["--device", args.device]
    if args.port:
        cmd += ["--port", args.port]
    return subprocess.call(cmd, cwd=REPO_ROOT)


def cmd_monitor(args: argparse.Namespace) -> int:
    try:
        import serial
    except ImportError:
        print("error: monitor needs pyserial - install with: pip install pyserial",
              file=sys.stderr)
        return 1

    port = args.port
    if not port:
        try:
            cfg = load_device_config(args.config)
            port = resolve_port(cfg)
        except FileNotFoundError:
            pass
    if not port:
        print("error: no port given and none could be resolved "
              f"(pass --port, or set device.port in {args.config})", file=sys.stderr)
        return 1

    print(f"Monitoring {port} @ {args.baud} (Ctrl-C to exit)")
    try:
        with serial.Serial(port, args.baud, timeout=1) as ser:
            while True:
                line = ser.readline()
                if line:
                    sys.stdout.write(line.decode("utf-8", errors="replace"))
                    sys.stdout.flush()
    except KeyboardInterrupt:
        print()
        return 0
    except serial.SerialException as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


APP_C_TEMPLATE = '''#include "app_framework.h"
#include "app_kit.h"

extern const app_icon_t {name}_app_icon;

static app_ui_t g_ui;

static void on_back(void* app, void* user) {{
    (void)user;
    app_request_exit(app);
}}

static void on_init(void* app) {{
    app_ui_config_t cfg;
    app_ui_config_ui(&cfg, "{title}", "Bk:back");
    app_ui_init(&g_ui, app, &cfg);

    app_ui_bind_keys(&g_ui, (app_ui_key_def_t[]){{
        {{SIM_KEY_ESCAPE, on_back, NULL}},
        {{0, NULL, NULL}},
    }});

    APP_INFO("{name} ready");
}}

static void on_frame(void* app) {{
    (void)app;
    app_ui_begin_frame(&g_ui);
    app_ui_line(&g_ui, 0, "{title}");
    app_ui_end_frame(&g_ui);
}}

static void on_cleanup(void* app) {{
    (void)app;
    app_ui_deinit(&g_ui);
}}

APP_DEFINE({name}_app, "{name}", .version = "1.0.0", .author = "ArdubotOS",
           .description = "{title}", .icon = &{name}_app_icon, .fps = 30,
           .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
'''

ICON_C_TEMPLATE = '''#include "icons.h"

/* {name} app icon - placeholder, replace with real artwork. */
const app_icon_t {name}_app_icon = {{{{
    0x0000, 0x1FF8, 0x1008, 0x1008, 0x1008, 0x1008, 0x1008, 0x1008,
    0x1008, 0x1008, 0x1008, 0x1008, 0x1008, 0x1FF8, 0x0000, 0x0000,
}}}};
'''


def cmd_create_app(args: argparse.Namespace) -> int:
    name = args.name.strip().lower()
    if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
        print("error: app name must be lowercase alnum/underscore, starting with a letter",
              file=sys.stderr)
        return 1

    app_dir = REPO_ROOT / "apps" / "stdapps" / name
    if app_dir.exists():
        print(f"error: {app_dir} already exists", file=sys.stderr)
        return 1

    title = name.upper()
    app_dir.mkdir(parents=True)
    (app_dir / f"{name}_app.c").write_text(APP_C_TEMPLATE.format(name=name, title=title))
    (app_dir / f"{name}_icon.c").write_text(ICON_C_TEMPLATE.format(name=name))

    cmake_path = REPO_ROOT / "apps" / "stdapps" / "CMakeLists.txt"
    cmake_text = cmake_path.read_text()
    cmake_text = cmake_text.replace(
        "    demo/demo_icon.c\n)",
        f"    demo/demo_icon.c\n    {name}/{name}_app.c\n    {name}/{name}_icon.c\n)",
        1,
    )
    cmake_text = cmake_text.replace(
        "    ${CMAKE_CURRENT_SOURCE_DIR}/demo\n",
        f"    ${{CMAKE_CURRENT_SOURCE_DIR}}/demo\n    ${{CMAKE_CURRENT_SOURCE_DIR}}/{name}\n",
        1,
    )
    cmake_path.write_text(cmake_text)

    print(f"Created apps/stdapps/{name}/{name}_app.c (+ icon) and registered it in "
          f"apps/stdapps/CMakeLists.txt.")
    print()
    print("To actually install it, add to sim/sim_main.c:")
    print(f'  extern app_manifest_t* {name}_app_manifest;')
    print(f'  app_install_manifest({name}_app_manifest, "{name}");')
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="ardubot", description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)

    p_flash = sub.add_parser("flash", help="build + flash the device_config.yaml target")
    p_flash.add_argument("--config", default="device_config.yaml")
    p_flash.add_argument("--device")
    p_flash.add_argument("--port")
    p_flash.set_defaults(func=cmd_flash)

    p_monitor = sub.add_parser("monitor", help="open a serial monitor")
    p_monitor.add_argument("--config", default="device_config.yaml")
    p_monitor.add_argument("--port")
    p_monitor.add_argument("--baud", type=int, default=115200)
    p_monitor.set_defaults(func=cmd_monitor)

    p_create = sub.add_parser("create-app", help="scaffold a new stdapp")
    p_create.add_argument("name")
    p_create.set_defaults(func=cmd_create_app)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
