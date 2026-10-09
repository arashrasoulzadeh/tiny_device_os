#!/usr/bin/env python3
"""PlatformIO pre-script to set enabled apps, main app, and features from device_config.yaml.

This script is called by PlatformIO before building. It reads device_config.yaml,
resolves the enabled apps, and sets the appropriate compiler defines and source filters.
"""

import os
import sys
from pathlib import Path

# Add scripts directory to path
REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = REPO_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))

from device_config import load_device_config, resolve_apps, load_stdapp_packages  # noqa: E402

# PlatformIO provides Import() when running as a pre-script
try:
    Import("env")
except NameError:
    # Not running under PlatformIO, create a mock env for testing
    class MockEnv:
        def __init__(self):
            self.cpp_defines = {}
            self.src_filter = []
            self.cpp_path = []
            self.project_dir = str(REPO_ROOT)
        
        def Append(self, **kwargs):
            for key, value in kwargs.items():
                if key == "CPPDEFINES":
                    for item in value:
                        if isinstance(item, tuple):
                            self.cpp_defines[item[0]] = item[1]
                        else:
                            self.cpp_defines[item] = 1
                elif key == "SRC_FILTER":
                    self.src_filter.extend(value)
                elif key == "CPPPATH":
                    self.cpp_path.extend(value)
                print(f"MockEnv.Append: {key}={value}")
        
        def get(self, key, default=None):
            if key == "PROJECT_DIR":
                return self.project_dir
            return default
    
    env = MockEnv()

def get_config_path():
    """Get the path to device_config.yaml."""
    # PlatformIO sets PROJECT_DIR to the project root
    project_dir = Path(env.get("PROJECT_DIR", str(REPO_ROOT)))
    config_path = project_dir / "device_config.yaml"
    if not config_path.exists():
        # Try relative to script
        config_path = REPO_ROOT / "device_config.yaml"
    return config_path

def main():
    config_path = get_config_path()
    if not config_path.exists():
        print(f"Warning: {config_path} not found, using defaults")
        return

    try:
        cfg = load_device_config(str(config_path))
    except Exception as e:
        print(f"Error loading device_config.yaml: {e}")
        return

    # Resolve enabled apps
    enabled_apps, excluded = resolve_apps(cfg)
    
    # Launcher is always enabled (required by stdapps_install)
    if "launcher" not in enabled_apps:
        enabled_apps.append("launcher")

    print(f"Enabled apps: {enabled_apps}")

    # Set ARDUBOT_APP_<NAME>_ENABLED defines
    for app_name in enabled_apps:
        # Only define for apps that stdapps_install knows about
        known_apps = {
            "launcher", "counter", "info", "stopwatch", "pong", "widgets",
            "pomodoro", "taskmgr", "clock", "sensors"
        }
        if app_name in known_apps:
            define_name = f"ARDUBOT_APP_{app_name.upper()}_ENABLED"
            env.Append(CPPDEFINES=[(define_name, 1)])
            print(f"  Defined {define_name}=1")

    # Add source files for each enabled app
    stdapps_dir = REPO_ROOT / "apps" / "stdapps"
    for app_name in enabled_apps:
        app_dir = stdapps_dir / app_name
        if app_dir.exists():
            # Add all .c files in the app directory
            for c_file in app_dir.glob("*.c"):
                rel_path = c_file.relative_to(REPO_ROOT)
                env.Append(SRC_FILTER=[f"+<{rel_path}>"])
                print(f"  Added source: {rel_path}")

    # Set main app if specified
    main_app = cfg.get("main_app")
    if main_app and main_app in enabled_apps:
        env.Append(CPPDEFINES=[("ARDUBOT_MAIN_APP", f'"{main_app}"')])
        print(f"  Main app: {main_app}")

    # Handle features
    features = cfg.get("features", {})
    if features:
        # Generate web_features.h
        features_header = REPO_ROOT / "build" / "generated" / "web_features.h"
        features_header.parent.mkdir(parents=True, exist_ok=True)
        
        lines = [
            "/* Auto-generated from device_config.yaml features — do not edit */",
            "#pragma once",
            "",
        ]
        
        for symbol, value in features.items():
            if isinstance(value, bool):
                lines.append(f"#define {symbol} {1 if value else 0}")
            elif isinstance(value, int):
                lines.append(f"#define {symbol} {value}")
            else:
                lines.append(f'#define {symbol} "{value}"')
        
        features_header.write_text("\n".join(lines) + "\n")
        print(f"Generated {features_header.relative_to(REPO_ROOT)}")
        
        # Add include path for generated headers
        env.Append(CPPPATH=[str(features_header.parent)])

    # Also add ARDUBOT_PIO define
    env.Append(CPPDEFINES=[("ARDUBOT_PIO", 1)])

if __name__ == "__main__":
    main()