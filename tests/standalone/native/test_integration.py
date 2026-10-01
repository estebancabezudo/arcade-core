"""Standalone generators, relocation and non-destructive installation regressions."""
import hashlib
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
RPI = ROOT / "standalone/platforms/raspberry_pi"
SPEC = importlib.util.spec_from_file_location("native_rom_packager", RPI / "scripts/pack_roms.py")
PACKAGER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGER)


class NativeIntegrationTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "build").mkdir(exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=ROOT / "build")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.roms = self.base / "roms"
        shutil.copytree(ROOT / "common/roms/pacman", self.roms)
        self.output = self.base / "roms.c"

    def run_command(self, command):
        return subprocess.run(command, cwd=self.base, text=True,
                              capture_output=True, check=False)

    def test_reproducible_roms_preserve_timestamps(self):
        PACKAGER.pack(self.roms, self.output)
        payload = self.output.read_bytes()
        before = self.output.stat().st_mtime_ns
        PACKAGER.pack(self.roms, self.output)
        self.assertEqual(payload, self.output.read_bytes())
        self.assertEqual(before, self.output.stat().st_mtime_ns)
        self.assertEqual(payload.count(b"static const uint8_t"), 7)
        self.assertIn(b"raspberry_pacman_roms", payload)

    def test_corrupt_rom_does_not_overwrite_output(self):
        PACKAGER.pack(self.roms, self.output)
        before = self.output.read_bytes()
        rom = self.roms / "pacman.6e"
        data = bytearray(rom.read_bytes())
        data[10] ^= 1
        rom.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "CRC32"):
            PACKAGER.pack(self.roms, self.output)
        self.assertEqual(before, self.output.read_bytes())

    def test_wrong_size_creates_no_output(self):
        (self.roms / "pacman.6f").write_bytes(b"bad")
        with self.assertRaisesRegex(ValueError, "expected"):
            PACKAGER.pack(self.roms, self.output)
        self.assertFalse(self.output.exists())

    def test_missing_rom_creates_no_output(self):
        (self.roms / "pacman.6h").unlink()
        with self.assertRaises(FileNotFoundError):
            PACKAGER.pack(self.roms, self.output)
        self.assertFalse(self.output.exists())

    def test_rom_cli_from_another_working_directory(self):
        result = self.run_command([sys.executable, "-B", str(RPI / "scripts/pack_roms.py"),
                                   str(self.roms), str(self.output)])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(PACKAGER.ROOT, ROOT)
        self.assertTrue(self.output.is_file())

    def test_amalgamation_uses_common_sources_and_is_reproducible(self):
        output = self.base / "pacman.c"
        command = [sys.executable, "-B", str(ROOT / "standalone/tools/generate_pacman.py"),
                   "--output", str(output)]
        first = self.run_command(command)
        self.assertEqual(first.returncode, 0, first.stderr)
        before = output.stat().st_mtime_ns
        second = self.run_command(command)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(before, output.stat().st_mtime_ns)
        text = output.read_text(encoding="utf-8")
        self.assertIn("BEGIN common/devices/z80/z80.c", text)
        self.assertEqual(output.read_bytes(), (ROOT / "build/standalone/native/pacman.c").read_bytes())

    def make_image(self):
        image = self.base / "image"
        image.mkdir()
        (image / "kernel8.img").write_bytes(b"test image, not executable")
        (image / "config.txt").write_text("kernel=kernel8.img\n", encoding="utf-8")
        hashes = [hashlib.sha256((image / n).read_bytes()).hexdigest() + "  " + n
                  for n in ("kernel8.img", "config.txt")]
        (image / "SHA256SUMS").write_text("\n".join(hashes) + "\n", encoding="utf-8")
        return image

    def test_installer_preserves_firmware_and_backs_up_both_files(self):
        image = self.make_image()
        boot = self.base / "boot"
        boot.mkdir()
        (boot / "config.txt").write_bytes(b"previous config")
        (boot / "kernel8.img").write_bytes(b"previous kernel")
        (boot / "firmware.dat").write_bytes(b"do not replace")
        result = self.run_command(["bash", str(RPI / "scripts/install_image.sh"), str(image), str(boot)])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((boot / "config.txt").read_bytes(), (image / "config.txt").read_bytes())
        self.assertEqual((boot / "kernel8.img").read_bytes(), (image / "kernel8.img").read_bytes())
        self.assertEqual((boot / "firmware.dat").read_bytes(), b"do not replace")
        backups = list((boot / "arcade-core-backup").iterdir())
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / "config.txt").read_bytes(), b"previous config")
        self.assertEqual((backups[0] / "kernel8.img").read_bytes(), b"previous kernel")

    def test_failed_checksum_does_not_touch_destination(self):
        image = self.make_image()
        (image / "kernel8.img").write_bytes(b"corrupt")
        boot = self.base / "boot"
        boot.mkdir()
        result = self.run_command(["bash", str(RPI / "scripts/install_image.sh"), str(image), str(boot)])
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(list(boot.iterdir()), [])

    def test_installer_refuses_symlink_destination(self):
        image = self.make_image()
        boot = self.base / "boot"
        boot.mkdir()
        outside = self.base / "outside"
        outside.write_bytes(b"must survive")
        (boot / "config.txt").symlink_to(outside)
        result = self.run_command(["bash", str(RPI / "scripts/install_image.sh"), str(image), str(boot)])
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(outside.read_bytes(), b"must survive")
        self.assertFalse((boot / "kernel8.img").exists())

    def test_prepared_example_installs_from_another_working_directory(self):
        boot = self.base / "boot"
        boot.mkdir()
        result = self.run_command(["bash", str(RPI / "scripts/install_pi4_example.sh"), str(boot)])
        self.assertEqual(result.returncode, 0, result.stderr)
        example = ROOT / "build/standalone/raspberry_pi/examples/pi4_usb"
        self.assertEqual((boot / "kernel8.img").read_bytes(), (example / "kernel8.img").read_bytes())
        self.assertEqual((boot / "config.txt").read_bytes(), (example / "config.txt").read_bytes())

    def test_model_builds_have_separate_outputs(self):
        for model, cpu in ((3, "cortex-a53"), (4, "cortex-a72")):
            result = self.run_command(["make", "-C", str(RPI), "-B", "-n",
                                       f"MODEL={model}", "TOOLCHAIN=clang"])
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(f"-mcpu={cpu}", result.stdout)
            self.assertIn(f"/build/standalone/raspberry_pi/model{model}/clang/kernel8.img", result.stdout)
            self.assertIn("/common/roms/pacman", result.stdout)
        result = self.run_command(["make", "-C", str(RPI), "-n", "MODEL=5"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("MODEL must be 3 or 4", result.stderr)


if __name__ == "__main__":
    unittest.main()
