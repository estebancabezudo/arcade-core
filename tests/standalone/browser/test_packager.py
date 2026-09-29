"""Regression tests for local ROM packaging; no network or compiler required."""

import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "scripts/build-pacman-web.py"
SPEC = importlib.util.spec_from_file_location("pacman_packager", SCRIPT)
PACKAGER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGER)


class PackagerTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT / "build")
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.roms = self.base / "roms"
        shutil.copytree(ROOT / "common/roms/pacman", self.roms)
        self.output = self.base / "output"

    def test_reproducible_portable_output(self):
        PACKAGER.build(self.roms, self.output)
        other = self.base / "other"
        PACKAGER.build(self.roms, other)
        self.assertEqual({p.name for p in self.output.iterdir()}, {"index.html", "pacman.js"})
        for name in ("index.html", "pacman.js"):
            self.assertEqual((self.output / name).read_bytes(), (other / name).read_bytes())
        page = (self.output / "index.html").read_text(encoding="utf-8")
        self.assertIn('src="./pacman.js"', page)
        self.assertIn('charset="utf-8"', page)
        self.assertIn("screenOnly: true", page)

    def test_wrong_crc_does_not_overwrite_previous_output(self):
        PACKAGER.build(self.roms, self.output)
        before = (self.output / "pacman.js").read_bytes()
        rom = self.roms / "pacman.6e"
        data = bytearray(rom.read_bytes())
        data[0] ^= 1
        rom.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "CRC32"):
            PACKAGER.build(self.roms, self.output)
        self.assertEqual((self.output / "pacman.js").read_bytes(), before)

    def test_wrong_size_creates_no_distribution(self):
        (self.roms / "pacman.6e").write_bytes(b"invalid")
        with self.assertRaisesRegex(ValueError, "size"):
            PACKAGER.build(self.roms, self.output)
        self.assertFalse(self.output.exists())

    def test_missing_rom_creates_no_distribution(self):
        (self.roms / "pacman.6e").unlink()
        with self.assertRaises(FileNotFoundError):
            PACKAGER.build(self.roms, self.output)
        self.assertFalse(self.output.exists())

    def test_cli_can_run_outside_project_directory(self):
        result = subprocess.run(
            [sys.executable, "-B", str(SCRIPT), "--roms", str(self.roms),
             "--output", str(self.output)],
            cwd=self.base, capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.output / "pacman.js").is_file())


if __name__ == "__main__":
    unittest.main()
