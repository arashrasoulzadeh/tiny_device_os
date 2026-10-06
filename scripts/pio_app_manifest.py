"""PlatformIO pre-build: refresh app_manifests.inc from each app.json."""

import sys
from pathlib import Path

Import("env")  # noqa: F821  (SCons provides Import)

root = Path(env["PROJECT_DIR"])  # noqa: F821
sys.path.insert(0, str(root / "scripts"))

from app_manifest import write_manifest  # noqa: E402

write_manifest(root / "apps" / "stdapps", root / "build" / "generated" / "app_manifests.inc")
