"""Re-run the current script with the project virtualenv, when one exists.

scripts/install_deps.sh creates .venv-pio and installs PlatformIO and
pyserial there. System python3 on a fresh Linux machine does not have
those packages, and Debian's PEP 668 layout refuses a system-wide pip
install. Entry points call prefer_project_venv() so `make test` host
tools and `make usb` use that interpreter.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path


def prefer_project_venv() -> None:
    venv_py = Path(__file__).resolve().parents[1] / ".venv-pio" / "bin" / "python"
    if not venv_py.is_file():
        return
    try:
        if Path(sys.executable).resolve() == venv_py.resolve():
            return
    except OSError:
        return
    os.execv(str(venv_py), [str(venv_py), *sys.argv])
