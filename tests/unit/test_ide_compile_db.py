#!/usr/bin/env python3
"""Unit tests for scripts/ide_compile_db.py."""

from __future__ import annotations

import json
import stat
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from ide_compile_db import collect, merge_compile_commands  # noqa: E402


class MergeTests(unittest.TestCase):
    def test_first_database_wins_for_the_same_file(self) -> None:
        shared = "/tmp/proj/apps/pomodoro_app.c"
        merged = merge_compile_commands(
            [
                [
                    {
                        "directory": "/tmp/proj/build",
                        "file": shared,
                        "command": "cc -DHOST -c pomodoro_app.c",
                    }
                ],
                [
                    {
                        "directory": "/tmp/proj",
                        "file": shared,
                        "command": "riscv-gcc -DPIO -c pomodoro_app.c",
                    },
                    {
                        "directory": "/tmp/proj",
                        "file": "/tmp/proj/boards/main.cpp",
                        "command": "riscv-g++ -c main.cpp",
                    },
                ],
            ]
        )
        by_file = {entry["file"]: entry["command"] for entry in merged}
        self.assertEqual(by_file[str(Path(shared).resolve())], "cc -DHOST -c pomodoro_app.c")
        self.assertIn("riscv-g++ -c main.cpp", by_file[str(Path("/tmp/proj/boards/main.cpp").resolve())])
        self.assertEqual(len(merged), 2)

    def test_relative_file_is_resolved_against_directory(self) -> None:
        merged = merge_compile_commands(
            [
                [
                    {
                        "directory": "/tmp/proj/build",
                        "file": "apps/app.c",
                        "command": "cc -c apps/app.c",
                    }
                ]
            ]
        )
        self.assertEqual(merged[0]["file"], str(Path("/tmp/proj/build/apps/app.c").resolve()))

    def test_entries_without_a_file_are_skipped(self) -> None:
        merged = merge_compile_commands([[{"directory": "/tmp", "command": "cc"}]])
        self.assertEqual(merged, [])


class CollectTests(unittest.TestCase):
    def test_collect_keeps_sim_command_and_adds_device_only_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            sim = root / "sim.json"
            app = root / "apps" / "app.c"
            app.parent.mkdir()
            app.write_text("int app(void) { return 0; }\n", encoding="utf-8")
            sim.write_text(
                json.dumps(
                    [
                        {
                            "directory": str(root),
                            "file": str(app),
                            "command": "cc -DHOST -c app.c",
                        }
                    ]
                ),
                encoding="utf-8",
            )

            fake = root / "fake_pio.py"
            fake.write_text(
                textwrap.dedent(
                    """\
                    #!/usr/bin/env python3
                    import json, sys
                    from pathlib import Path
                    env = sys.argv[sys.argv.index("-e") + 1]
                    project = Path(sys.argv[sys.argv.index("-d") + 1])
                    board = project / "boards" / f"{env}.cpp"
                    board.parent.mkdir(exist_ok=True)
                    board.write_text("void setup() {}\\n", encoding="utf-8")
                    (project / "compile_commands.json").write_text(json.dumps([
                        {"directory": str(project), "file": str(board), "command": f"c++ -D{env} -c"},
                        {"directory": str(project), "file": str(project / "apps" / "app.c"),
                         "command": "c++ -DPIO -c"},
                    ]), encoding="utf-8")
                    """
                ),
                encoding="utf-8",
            )
            fake.chmod(fake.stat().st_mode | stat.S_IEXEC)

            out = root / "compile_commands.json"
            rc = collect(
                root=root,
                sim_db=sim,
                envs=["esp32-c6"],
                pio=[sys.executable, str(fake)],
                out=out,
            )
            self.assertEqual(rc, 0)
            entries = json.loads(out.read_text(encoding="utf-8"))
            by_name = {Path(entry["file"]).name: entry["command"] for entry in entries}
            self.assertEqual(by_name["app.c"], "cc -DHOST -c app.c")
            self.assertEqual(by_name["esp32-c6.cpp"], "c++ -Desp32-c6 -c")
            self.assertTrue((root / "build" / "compile_commands.json").is_file())
            self.assertFalse((root / "build" / "ide" / "esp32-c6.json").stat().st_size == 0)


if __name__ == "__main__":
    unittest.main()
