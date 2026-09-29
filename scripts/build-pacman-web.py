#!/usr/bin/env python3
"""Build a self-contained browser bundle from the template and local ROM files."""

import argparse
import base64
import json
from pathlib import Path
import re
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "standalone/platforms/browser"
MARKER = '"__PACMAN_ROM_BASE64__"'
SCRIPT_PATH = "../../../build/standalone/browser/pacman.js"


def atomic_write(path: Path, data: str) -> None:
    """Replace a generated file only after its contents have been written."""
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n",
                                         dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
        temporary.chmod(0o644)
        temporary.replace(path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def build(rom_directory: Path, output: Path) -> None:
    template = (SOURCE / "pacman.template.js").read_text(encoding="utf-8")
    match = re.search(r"^const ROM_MANIFEST = (\[.*\]);$", template, re.MULTILINE)
    if match is None or template.count(MARKER) != 1:
        raise ValueError("Invalid ROM manifest or payload placeholder in template")
    payload = bytearray()
    for entry in json.loads(match.group(1)):
        name = entry["name"]
        if Path(name).name != name or entry["offset"] != len(payload):
            raise ValueError(f"Invalid manifest entry: {name}")
        data = (rom_directory / name).read_bytes()
        if len(data) != entry["size"]:
            raise ValueError(f"Wrong ROM size: {name}; expected {entry['size']}")
        if zlib.crc32(data) != entry["crc32"]:
            raise ValueError(f"Wrong ROM CRC32: {name}")
        payload.extend(data)
    encoded = base64.b64encode(payload).decode("ascii")
    bundle = template.replace(MARKER, json.dumps(encoded))
    page = (SOURCE / "index.html").read_text(encoding="utf-8")
    if page.count(SCRIPT_PATH) != 1:
        raise ValueError("Expected exactly one generated-bundle path in index.html")
    page = page.replace(SCRIPT_PATH, "./pacman.js")
    output.mkdir(parents=True, exist_ok=True)
    atomic_write(output / "pacman.js", bundle)
    atomic_write(output / "index.html", page)
    print(f"Built {output / 'index.html'}")
    print(f"Embedded {len(payload)} ROM bytes; no runtime ROM downloads.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--roms", type=Path, default=ROOT / "common/roms/pacman")
    parser.add_argument("--output", type=Path, default=ROOT / "build/standalone/browser")
    args = parser.parse_args()
    try:
        build(args.roms.resolve(), args.output.resolve())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"Pac-Man web build failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
