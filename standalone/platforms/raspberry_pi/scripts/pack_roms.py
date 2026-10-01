#!/usr/bin/env python3
"""Pack the shared local Pac-Man ROM set into an ignored C translation unit."""
import argparse
from pathlib import Path
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[4]
EXPECTED = [
    ("pacman.6e", 0x1000, 0xc1e6ab10),
    ("pacman.6f", 0x1000, 0x1a6fb2d4),
    ("pacman.6h", 0x1000, 0xbcdd1beb),
    ("pacman.6j", 0x1000, 0x817d94e3),
    ("pacman.5e", 0x1000, 0x0c944964),
    ("pacman.5f", 0x1000, 0x958fedf9),
    ("82s123.7f", 0x0020, 0x2fc650bd),
    ("82s126.4a", 0x0100, 0x3eb3a8e4),
    ("82s126.1m", 0x0100, 0xa9cc86bf),
    ("82s126.3m", 0x0100, 0x77245b66),
]


def array(name: str, data: bytes) -> str:
    lines = ["    " + ", ".join(f"0x{x:02x}" for x in data[i:i + 16]) + ","
             for i in range(0, len(data), 16)]
    return f"static const uint8_t {name}[{len(data)}] = {{\n" + "\n".join(lines) + "\n};\n"


def pack(directory: Path, output: Path) -> None:
    values = {}
    for name, size, crc in EXPECTED:
        data = (directory / name).read_bytes()
        if len(data) != size:
            raise ValueError(f"{name}: expected {size} bytes, found {len(data)}")
        if zlib.crc32(data) != crc:
            raise ValueError(f"{name}: CRC32 mismatch")
        values[name] = data
    regions = {
        "program": b"".join(values[n] for n in ("pacman.6e", "pacman.6f", "pacman.6h", "pacman.6j")),
        "tile": values["pacman.5e"], "sprite": values["pacman.5f"],
        "palette": values["82s123.7f"], "color_lookup": values["82s126.4a"],
        "sound_waveform": values["82s126.1m"], "sound_timing": values["82s126.3m"],
    }
    content = ['/* GENERATED from user-supplied Pac-Man ROMs. DO NOT COMMIT. */\n',
               '#include "pacman.h"\n\n']
    content.extend(array("rom_" + name, data) for name, data in regions.items())
    content.append("\nconst PacmanStandaloneRomSet raspberry_pacman_roms = {\n")
    for name in regions:
        content.append(f"    .{name} = rom_{name},\n")
        content.append(f"    .{name}_size = sizeof(rom_{name}),\n")
    content.append("};\n")
    payload = "".join(content).encode("utf-8")
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and output.read_bytes() == payload:
        return
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(payload)
        temporary.chmod(0o644)
        temporary.replace(output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom_directory", type=Path, nargs="?", default=ROOT / "common/roms/pacman")
    parser.add_argument("output", type=Path, nargs="?", default=ROOT / "build/standalone/raspberry_pi/generated/roms.c")
    args = parser.parse_args()
    try:
        pack(args.rom_directory.resolve(), args.output.resolve())
    except (OSError, ValueError) as error:
        print(f"ROM packaging failed: {error}", file=sys.stderr)
        return 1
    print(f"generated={args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
