#!/usr/bin/env python3
"""Parse firmware.map and PlatformIO size output for size reports."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


def parse_firmware_map(map_path: Path) -> dict[str, Any] | None:
    """Parse .map file and split contributions by component."""
    if not map_path.exists():
        return None
    
    content = map_path.read_text(encoding="utf-8", errors="ignore")
    
    # Initialize categories
    categories = {
        "kernel": {"text": 0, "data": 0, "bss": 0},
        "apps": {},
        "runtime": {"text": 0, "data": 0, "bss": 0},
        "other": {"text": 0, "data": 0, "bss": 0},
    }
    
    # Known stdapp names
    stdapp_names = {
        "launcher", "counter", "info", "stopwatch", "pong", "widgets",
        "pomodoro", "taskmgr", "clock", "sensors"
    }
    
    # Parse object file sections
    # Map file format: .section  start  size  objfile
    # We look for lines like:  .text  0x00000000  0x1234  kernel/scheduler.c.o
    
    # Simpler approach: look for object file contributions
    # The map file has sections listing each object file and its symbols
    
    obj_pattern = re.compile(
        r'\.(\w+)\s+0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)\s+(\S+)'
    )
    
    for match in obj_pattern.finditer(content):
        section = match.group(1)
        size_hex = match.group(2)
        obj_file = match.group(3)
        
        try:
            size = int(size_hex, 16)
        except ValueError:
            continue
        
        if size == 0:
            continue
        
        # Categorize by object file path
        cat, app_name = categorize_object(obj_file, stdapp_names)
        
        if cat == "apps" and app_name:
            if app_name not in categories["apps"]:
                categories["apps"][app_name] = {"text": 0, "data": 0, "bss": 0}
            categories["apps"][app_name][section] = categories["apps"][app_name].get(section, 0) + size
        else:
            categories[cat][section] = categories[cat].get(section, 0) + size
    
    # Calculate totals
    result = {
        "categories": {},
        "total_flash": 0,
        "total_ram": 0,
    }
    
    for cat_name, sections in categories.items():
        if cat_name == "apps":
            result["categories"]["apps"] = {}
            for app_name, app_sections in sections.items():
                text = app_sections.get("text", 0)
                data = app_sections.get("data", 0)
                bss = app_sections.get("bss", 0)
                result["categories"]["apps"][app_name] = {
                    "text": text,
                    "data": data,
                    "bss": bss,
                    "flash": text + data,
                    "ram": data + bss,
                }
                result["total_flash"] += text + data
                result["total_ram"] += data + bss
        else:
            text = sections.get("text", 0)
            data = sections.get("data", 0)
            bss = sections.get("bss", 0)
            result["categories"][cat_name] = {
                "text": text,
                "data": data,
                "bss": bss,
                "flash": text + data,
                "ram": data + bss,
            }
            result["total_flash"] += text + data
            result["total_ram"] += data + bss
    
    return result


def categorize_object(obj_file: str, stdapp_names: set[str]) -> tuple[str, str | None]:
    """Categorize an object file path."""
    obj_lower = obj_file.lower()
    
    # Kernel
    if "kernel/" in obj_lower or "hal/" in obj_lower:
        return "kernel", None
    
    # Apps
    if "apps/stdapps/" in obj_lower:
        for app in stdapp_names:
            if f"apps/stdapps/{app}/" in obj_lower:
                return "apps", app
        return "runtime", None
    
    if "apps/" in obj_lower:
        return "runtime", None
    
    return "other", None


def parse_platformio_size_output(output: str) -> dict[str, int] | None:
    """Parse PlatformIO memory summary output."""
    # Look for lines like:
    # RAM:   [==        ]  12.3% (used 12345 bytes from 100000 bytes)
    # Flash: [===       ]  23.4% (used 23456 bytes from 100000 bytes)
    
    ram_match = re.search(r'RAM:\s+\[.*?\]\s+\d+\.?\d*%\s+\(used\s+(\d+)\s+bytes\s+from\s+(\d+)\s+bytes\)', output)
    flash_match = re.search(r'Flash:\s+\[.*?\]\s+\d+\.?\d*%\s+\(used\s+(\d+)\s+bytes\s+from\s+(\d+)\s+bytes\)', output)
    
    result = {}
    if ram_match:
        result["ram_used"] = int(ram_match.group(1))
        result["ram_total"] = int(ram_match.group(2))
    if flash_match:
        result["flash_used"] = int(flash_match.group(1))
        result["flash_total"] = int(flash_match.group(2))
    
    return result if result else None


def generate_size_report(map_path: Path, pio_output: str | None = None) -> dict[str, Any] | None:
    """Generate complete size report from map file and PlatformIO output."""
    map_data = parse_firmware_map(map_path)
    if not map_data:
        return None
    
    report = {
        "measured": True,
        "categories": map_data["categories"],
        "total_flash": map_data["total_flash"],
        "total_ram": map_data["total_ram"],
    }
    
    if pio_output:
        pio_data = parse_platformio_size_output(pio_output)
        if pio_data:
            report["platformio"] = pio_data
    
    return report


def save_size_report(report: dict[str, Any], out_path: Path) -> None:
    """Save size report to JSON file."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(report, indent=2))


def load_size_report(path: Path) -> dict[str, Any] | None:
    """Load size report from JSON file."""
    if not path.exists():
        return None
    return json.loads(path.read_text())


def compute_delta(prev: dict[str, Any], curr: dict[str, Any]) -> dict[str, Any]:
    """Compute delta between two size reports."""
    delta = {"categories": {}}
    
    # Compare totals
    for key in ["total_flash", "total_ram"]:
        if key in prev and key in curr:
            delta[key] = curr[key] - prev[key]
    
    # Compare categories
    for cat in ["kernel", "runtime", "other"]:
        if cat in prev.get("categories", {}) and cat in curr.get("categories", {}):
            delta["categories"][cat] = {}
            for key in ["text", "data", "bss", "flash", "ram"]:
                if key in prev["categories"][cat] and key in curr["categories"][cat]:
                    delta["categories"][cat][key] = (
                        curr["categories"][cat][key] - prev["categories"][cat][key]
                    )
    
    # Compare apps
    prev_apps = prev.get("categories", {}).get("apps", {})
    curr_apps = curr.get("categories", {}).get("apps", {})
    all_apps = set(prev_apps.keys()) | set(curr_apps.keys())
    delta["categories"]["apps"] = {}
    for app in all_apps:
        if app in prev_apps and app in curr_apps:
            delta["categories"]["apps"][app] = {}
            for key in ["text", "data", "bss", "flash", "ram"]:
                if key in prev_apps[app] and key in curr_apps[app]:
                    delta["categories"]["apps"][app][key] = (
                        curr_apps[app][key] - prev_apps[app][key]
                    )
        elif app in curr_apps:
            delta["categories"]["apps"][app] = curr_apps[app]
    
    return delta


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        map_file = Path(sys.argv[1])
        if map_file.exists():
            report = generate_size_report(map_file)
            if report:
                print(json.dumps(report, indent=2))
            else:
                print("Failed to parse map file")
        else:
            print(f"Map file not found: {map_file}")