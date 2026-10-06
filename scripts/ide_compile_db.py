#!/usr/bin/env python3
"""Merge host and device compilation databases for IDE indexing.

clangd and Cursor resolve includes, completions, and Find All References from
one compile_commands.json. CMake writes the simulator database under
build/<arch>/<build>/, and PlatformIO writes a separate one per env at the
repo root (overwriting it). This script keeps the simulator command for any
file both builds compile, then appends device-only translation units
(board sketches, the kernel boot wrapper) so those files index too.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

DEFAULT_PIO_ENVS = ("esp32-c6", "esp32-c6-kernel", "nodemcu")


def load_entries(path: Path) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise ValueError(f"{path} is not a compile_commands.json array")
    return [entry for entry in data if isinstance(entry, dict)]


def entry_file(entry: dict) -> str | None:
    raw = entry.get("file")
    if not isinstance(raw, str) or not raw:
        return None
    path = Path(raw)
    if not path.is_absolute():
        directory = entry.get("directory")
        if isinstance(directory, str) and directory:
            path = Path(directory) / path
    return str(path.resolve())


def merge_compile_commands(documents: list[list[dict]]) -> list[dict]:
    """First database wins when the same source file appears twice."""
    merged: list[dict] = []
    seen: set[str] = set()
    for document in documents:
        for entry in document:
            key = entry_file(entry)
            if key is None or key in seen:
                continue
            seen.add(key)
            kept = dict(entry)
            kept["file"] = key
            merged.append(kept)
    return merged


def write_database(path: Path, entries: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")


def find_platformio(explicit: str | None) -> list[str] | None:
    if explicit:
        return [explicit]
    venv = ROOT / ".venv-pio" / "bin" / "platformio"
    if venv.is_file():
        return [str(venv)]
    found = shutil.which("platformio")
    if found:
        return [found]
    return None


def collect(
    *,
    root: Path,
    sim_db: Path | None,
    envs: list[str],
    pio: list[str] | None,
    out: Path,
) -> int:
    ide_dir = root / "build" / "ide"
    ide_dir.mkdir(parents=True, exist_ok=True)
    parts: list[list[dict]] = []

    if sim_db is not None and sim_db.is_file():
        parts.append(load_entries(sim_db))
        print(f"sim: {sim_db} ({len(parts[-1])} files)")
    else:
        print(f"warning: no simulator database at {sim_db}", file=sys.stderr)

    if envs and pio is None:
        print("warning: platformio not found; device sources will not be indexed", file=sys.stderr)
    elif pio is not None:
        for env in envs:
            print(f"PlatformIO compiledb: {env}")
            rc = subprocess.call(
                [*pio, "run", "-e", env, "-t", "compiledb", "-d", str(root)],
                cwd=root,
            )
            produced = root / "compile_commands.json"
            dest = ide_dir / f"{env}.json"
            if rc != 0 or not produced.is_file():
                print(f"warning: compiledb failed for {env}", file=sys.stderr)
                if produced.is_file():
                    produced.unlink()
                continue
            shutil.move(produced, dest)
            entries = load_entries(dest)
            parts.append(entries)
            print(f"  {env}: {len(entries)} files")

    merged = merge_compile_commands(parts)
    write_database(out, merged)
    build_copy = root / "build" / "compile_commands.json"
    if out.resolve() != build_copy.resolve():
        write_database(build_copy, merged)
    print(f"wrote {out} ({len(merged)} files)")
    return 0 if merged else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    merge_p = sub.add_parser("merge", help="merge existing compile_commands.json files")
    merge_p.add_argument("--out", type=Path, required=True)
    merge_p.add_argument("inputs", nargs="+", type=Path)

    collect_p = sub.add_parser("collect", help="refresh sim + PlatformIO databases and merge")
    collect_p.add_argument(
        "--sim",
        type=Path,
        default=ROOT / "build" / "sim" / "Debug" / "compile_commands.json",
    )
    collect_p.add_argument("--out", type=Path, default=ROOT / "compile_commands.json")
    collect_p.add_argument("--pio", default=None, help="platformio executable")
    collect_p.add_argument(
        "--env",
        action="append",
        dest="envs",
        default=None,
        help="PlatformIO env to index (repeatable; default: esp32-c6, esp32-c6-kernel, nodemcu)",
    )
    collect_p.add_argument(
        "--no-pio",
        action="store_true",
        help="only merge the simulator database",
    )

    args = parser.parse_args(argv)
    if args.cmd == "merge":
        documents = [load_entries(path) for path in args.inputs]
        merged = merge_compile_commands(documents)
        write_database(args.out, merged)
        print(f"wrote {args.out} ({len(merged)} files)")
        return 0

    envs = [] if args.no_pio else (args.envs if args.envs is not None else list(DEFAULT_PIO_ENVS))
    pio = None if args.no_pio else find_platformio(args.pio)
    return collect(root=ROOT, sim_db=args.sim, envs=envs, pio=pio, out=args.out)


if __name__ == "__main__":
    sys.exit(main())
