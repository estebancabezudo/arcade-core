#!/usr/bin/env python3
"""Build the browser distribution from versioned templates and local ROM files.

Architecture and design
The packager is the only source-to-distribution boundary. It injects the private ROM
payload and versioned legacy CSS into the JavaScript bundle, and copies the same CSS
plus the standalone page loader as deterministic external CSP-safe artifacts.
"""

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
APP_CSS_MARKER = '"__PACMAN_APP_CSS__"'
SCREEN_CSS_MARKER = '"__PACMAN_SCREEN_CSS__"'
DISTRIBUTION_PREFIX = "../../../build/standalone/browser/"


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
    markers = (MARKER, APP_CSS_MARKER, SCREEN_CSS_MARKER)
    if match is None or any(template.count(marker) != 1 for marker in markers):
        raise ValueError("Invalid ROM manifest or distribution placeholder in template")
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
    app_css = (SOURCE / "pacman.css").read_text(encoding="utf-8")
    screen_css = (SOURCE / "pacman-screen.css").read_text(encoding="utf-8")
    bundle = (template
              .replace(MARKER, json.dumps(encoded))
              .replace(APP_CSS_MARKER, json.dumps(app_css))
              .replace(SCREEN_CSS_MARKER, json.dumps(screen_css)))
    page = (SOURCE / "index.html").read_text(encoding="utf-8")
    if page.count(DISTRIBUTION_PREFIX) != 3:
        raise ValueError("Expected generated distribution paths in index.html")
    page = page.replace(DISTRIBUTION_PREFIX, "./")
    output.mkdir(parents=True, exist_ok=True)
    atomic_write(output / "pacman.js", bundle)
    atomic_write(output / "index.html", page)
    for name in ("pacman.css", "pacman-screen.css", "pacman-page.css", "pacman-page.js"):
        atomic_write(output / name, (SOURCE / name).read_text(encoding="utf-8"))
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
