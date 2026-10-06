#!/usr/bin/env python3
"""ardubot - single entry point CLI for ArdubotOS.

    ardubot flash               build + flash the current device_config.yaml target
    ardubot monitor             open a serial monitor on the device's port
    ardubot create-app <name>   scaffold a new stdapp from the app_ui kit template
    ardubot install <path>      install an app.json-described app from a local dir
    ardubot list                list installed stdapps and their package metadata

Thin wrapper over the existing Makefile / scripts/*.py tooling - it exists so
there is one documented command instead of five different `make` incantations.

Package format (app.json, one per apps/stdapps/<name>/ directory):
    {
      "name": "mygame",        // must match the directory name
      "version": "1.0.0",
      "author": "you",
      "description": "...",
      "depends": []            // names of other installed stdapps, checked
                                // at install time; no fetching/resolution -
                                // there is no hosted registry yet, so this
                                // only installs from a local directory.
    }
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
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

APP_DEFINE({name}_app, "{name}", .icon = &{name}_app_icon, .fps = 30,
           .on_init = on_init, .on_frame = on_frame, .on_cleanup = on_cleanup)
'''

ICON_C_TEMPLATE = '''#include "icons.h"

/* {name} app icon - placeholder, replace with real artwork. */
const app_icon_t {name}_app_icon = {{{{
    0x0000, 0x1FF8, 0x1008, 0x1008, 0x1008, 0x1008, 0x1008, 0x1008,
    0x1008, 0x1008, 0x1008, 0x1008, 0x1008, 0x1FF8, 0x0000, 0x0000,
}}}};
'''

APP_NAME_RE = re.compile(r"[a-z][a-z0-9_]*")


def stdapps_dir() -> Path:
    return REPO_ROOT / "apps" / "stdapps"


def installed_packages() -> dict[str, dict]:
    """Maps stdapp name -> parsed app.json (missing manifest -> {})."""
    pkgs: dict[str, dict] = {}
    for d in sorted(stdapps_dir().iterdir()):
        if not d.is_dir():
            continue
        manifest = d / "app.json"
        pkgs[d.name] = json.loads(manifest.read_text()) if manifest.exists() else {}
    return pkgs


def register_stdapp(name: str) -> None:
    """Adds `name` to apps/stdapps/CMakeLists.txt's ARDUBOT_ALL_STDAPPS list.
    Sources are picked up by that file's own file(GLOB ...) per app
    directory, so create-app and install only need this one edit."""
    cmake_path = stdapps_dir() / "CMakeLists.txt"
    cmake_text = cmake_path.read_text()
    marker = "    demo\n)"
    if marker not in cmake_text:
        raise RuntimeError(f"{cmake_path}: expected marker {marker!r} not found - "
                            "edit ARDUBOT_ALL_STDAPPS there by hand")
    cmake_text = cmake_text.replace(marker, f"    demo\n    {name}\n)", 1)
    cmake_path.write_text(cmake_text)


def cmd_create_app(args: argparse.Namespace) -> int:
    name = args.name.strip().lower()
    if not re.fullmatch(APP_NAME_RE, name):
        print("error: app name must be lowercase alnum/underscore, starting with a letter",
              file=sys.stderr)
        return 1

    app_dir = stdapps_dir() / name
    if app_dir.exists():
        print(f"error: {app_dir} already exists", file=sys.stderr)
        return 1

    title = name.upper()
    app_dir.mkdir(parents=True)
    (app_dir / f"{name}_app.c").write_text(APP_C_TEMPLATE.format(name=name, title=title))
    (app_dir / f"{name}_icon.c").write_text(ICON_C_TEMPLATE.format(name=name))
    (app_dir / "app.json").write_text(json.dumps({
        "name": name,
        "version": "1.0.0",
        "author": "",
        "description": title,
        "depends": [],
        "min_display": {"width": 0, "height": 0},
    }, indent=2) + "\n")

    register_stdapp(name)

    print(f"Created apps/stdapps/{name}/ ({name}_app.c, {name}_icon.c, app.json) "
          f"and registered it in apps/stdapps/CMakeLists.txt.")
    print()
    print("To actually install it, add to sim/sim_main.c:")
    print(f'  extern app_manifest_t* {name}_app_manifest;')
    print(f'  app_install_manifest({name}_app_manifest, "{name}");')
    return 0


def cmd_install(args: argparse.Namespace) -> int:
    src = Path(args.source).expanduser().resolve()
    manifest_path = src / "app.json"
    if not manifest_path.is_file():
        print(f"error: {manifest_path} not found - a package needs an app.json "
              "(see `ardubot --help` for the format)", file=sys.stderr)
        return 1

    try:
        manifest = json.loads(manifest_path.read_text())
    except json.JSONDecodeError as exc:
        print(f"error: {manifest_path} is not valid JSON: {exc}", file=sys.stderr)
        return 1

    name = manifest.get("name", "")
    if not re.fullmatch(APP_NAME_RE, name):
        print(f"error: app.json's \"name\" ({name!r}) must be lowercase "
              "alnum/underscore, starting with a letter", file=sys.stderr)
        return 1
    if src.name != name:
        print(f"error: app.json says name={name!r} but the source directory is "
              f"{src.name!r} - rename one to match", file=sys.stderr)
        return 1

    dest = stdapps_dir() / name
    if dest.exists() and not args.force:
        print(f"error: apps/stdapps/{name} already exists (pass --force to overwrite)",
              file=sys.stderr)
        return 1

    installed = installed_packages()
    missing_deps = [d for d in manifest.get("depends", []) if d not in installed]
    if missing_deps:
        print(f"error: {name} depends on {missing_deps}, which "
              "aren't installed under apps/stdapps/ - install those first "
              "(no registry/fetching yet, so this only resolves local installs)",
              file=sys.stderr)
        return 1

    c_sources = sorted(p.name for p in src.glob("*.c"))
    if not c_sources:
        print(f"error: {src} has no .c files to install", file=sys.stderr)
        return 1

    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(src, dest)

    register_stdapp(name)

    print(f"Installed {name} {manifest.get('version', '?')} into apps/stdapps/{name}/ "
          f"({len(c_sources)} source file(s)) and added it to "
          "apps/stdapps/CMakeLists.txt's ARDUBOT_ALL_STDAPPS.")
    manifest_name = f"{name}_app_manifest"
    print()
    print("To actually install it, add to sim/sim_main.c:")
    print(f'  extern app_manifest_t* {manifest_name};')
    print(f'  app_install_manifest({manifest_name}, "{name}");')
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    (_) = args
    pkgs = installed_packages()
    if not pkgs:
        print("No stdapps installed.")
        return 0
    for name, meta in pkgs.items():
        version = meta.get("version", "?")
        deps = meta.get("depends", [])
        dep_str = f" (depends: {', '.join(deps)})" if deps else ""
        min_disp = meta.get("min_display") or {}
        min_w, min_h = min_disp.get("width", 0), min_disp.get("height", 0)
        fit_str = f" (needs >={min_w}x{min_h})" if min_w or min_h else ""
        desc = meta.get("description", "")
        print(f"{name:<16} {version:<10} {desc}{dep_str}{fit_str}")
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

    p_install = sub.add_parser("install", help="install an app.json app from a local dir")
    p_install.add_argument("source", help="path to a directory containing app.json")
    p_install.add_argument("--force", action="store_true",
                            help="overwrite an existing install of the same name")
    p_install.set_defaults(func=cmd_install)

    p_list = sub.add_parser("list", help="list installed stdapps")
    p_list.set_defaults(func=cmd_list)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
