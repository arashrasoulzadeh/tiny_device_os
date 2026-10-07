#!/usr/bin/env python3
"""Write build_info.h with the last 8 hex characters of HEAD."""

import argparse
import subprocess
from pathlib import Path


def git_hash(repo: Path) -> str:
    try:
        full = subprocess.check_output(
            ["git", "-C", str(repo), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError, OSError):
        return "00000000"
    hexchars = "".join(ch for ch in full if ch in "0123456789abcdefABCDEF")
    if len(hexchars) < 8:
        return "00000000"
    return hexchars[-8:].lower()


def write_build_info(repo: Path, out: Path) -> None:
    digest = git_hash(repo)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        "/* Auto-generated — do not edit */\n"
        "#pragma once\n"
        f'#define ARDUBOT_GIT_HASH "{digest}"\n',
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Write ARDUBOT_GIT_HASH header")
    parser.add_argument("--repo", default=".", help="Git checkout to read")
    parser.add_argument("-o", required=True, help="Output header path")
    args = parser.parse_args()
    write_build_info(Path(args.repo), Path(args.o))


if __name__ == "__main__":
    main()
