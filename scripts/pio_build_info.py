"""PlatformIO pre-build: write build/generated/build_info.h."""

import sys
from pathlib import Path

Import("env")  # noqa: F821  (SCons provides Import)

root = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(root / "scripts"))

from gen_build_info import write_build_info  # noqa: E402

write_build_info(root, root / "build" / "generated" / "build_info.h")
