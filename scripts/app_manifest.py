#!/usr/bin/env python3
"""Turn each stdapp's app.json into a C table the OS links at compile time."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

MANIFEST_FILENAME = "app.json"
DEFAULT_VERSION = "1.0.0"
DEFAULT_AUTHOR = "ArdubotOS"


def c_string(text: str) -> str:
    """A C string literal. json.dumps uses the same escapes C11 accepts."""
    return json.dumps(text, ensure_ascii=True)


def load_apps(stdapps_dir: Path) -> list[tuple[str, dict]]:
    apps: list[tuple[str, dict]] = []
    if not stdapps_dir.is_dir():
        return apps
    for directory in sorted(p for p in stdapps_dir.iterdir() if p.is_dir()):
        manifest = directory / MANIFEST_FILENAME
        if not manifest.is_file():
            continue
        meta = json.loads(manifest.read_text(encoding="utf-8"))
        if not isinstance(meta, dict):
            raise ValueError(f"{manifest} must be a JSON object")
        apps.append((directory.name, meta))
    return apps


def _text(meta: dict, key: str, fallback: str) -> str:
    value = meta.get(key)
    if value is None or value == "":
        return fallback
    return str(value)


def render_inc(apps: list[tuple[str, dict]]) -> str:
    lines = ["/* Generated from each stdapp app.json. Do not edit. */"]
    for name, meta in apps:
        version = _text(meta, "version", DEFAULT_VERSION)
        author = _text(meta, "author", DEFAULT_AUTHOR)
        description = _text(meta, "description", name)
        lines.append(
            "    { "
            + ", ".join(
                (
                    c_string(name),
                    c_string(version),
                    c_string(author),
                    c_string(description),
                )
            )
            + " },"
        )
    return "\n".join(lines) + "\n"


def write_manifest(stdapps_dir: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(render_inc(load_apps(stdapps_dir)), encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the stdapp manifest table")
    sub = parser.add_subparsers(dest="cmd", required=True)
    gen = sub.add_parser("gen", help="Write app_manifests.inc")
    gen.add_argument("--stdapps", default="apps/stdapps")
    gen.add_argument("-o", "--output", required=True)
    args = parser.parse_args(argv)
    write_manifest(Path(args.stdapps), Path(args.output))
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
