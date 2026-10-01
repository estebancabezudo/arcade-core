# Directory migration

[Español](README.es.md) | [Project README](../../README.md)

This package combines the previous reorganization of `arcade-core(2).zip` with the supplied `arcade-core-standalone-pacman-rpi4(2).zip`. It has no extra enclosing directory: its `Makefile`, `common/`, `modular/` and `standalone/` belong directly in the existing repository root.

Copy the new files first. Keep your existing `.git/` and any local-only files not present in the input snapshot. The package does not replace Git metadata, create commits or publish ROMs.

| Previous path | New path |
| --- | --- |
| `devices/` | `common/devices/` |
| `boards/` | `common/boards/` |
| `machines/` | `common/machines/` |
| `romsets/` | `common/romsets/` |
| `rtl/` | `common/rtl/` |
| `roms/pacman/` | `common/roms/pacman/` |
| `platform/host/` | `modular/platforms/host/` |
| `sim/pacman_video_system_sdl.cpp` | `modular/platforms/host/pacman_video_system_sdl.cpp` |
| Other `sim/*_tb.cpp` | `tests/rtl/` |
| `fpga/tangnano20k/` | `modular/platforms/fpga/tangnano20k/` |
| `tests/host/` | `tests/modular/host/` |
| Other C test directories | `tests/common/` |
| `standalone/platform.h` | `standalone/include/platform.h` |
| `standalone/browser/index.html` | `standalone/platforms/browser/index.html` |
| `standalone/browser/pacman.js` | `standalone/platforms/browser/pacman.template.js` + generated `build/standalone/browser/pacman.js` |
| `standalone/platforms/raspberry_pi/` | Imported hardware sources remain here; ROMs and generated outputs are relocated |

## Obsolete files to remove

[remove-old-paths.txt](remove-old-paths.txt) lists every old file path to remove, one relative path per line. [path-map.tsv](path-map.tsv) provides the corresponding destination. Remove those old files only after confirming the new copies exist; the JavaScript payload now lives in the generated bundle rather than the ROM-free template.

The obsolete directory roots are `devices/`, `boards/`, `machines/`, `romsets/`, `rtl/`, `roms/pacman/`, `platform/host/`, `sim/`, `fpga/tangnano20k/`, `tests/host/`, `tests/namco_wsg/`, `tests/pacman/`, `tests/pacman_audio/`, `tests/pacman_cpu/`, `tests/z80/`, and `standalone/browser/`, plus the file `standalone/platform.h`. Remove emptied parent directories only after checking for additional local files. Do not recursively delete all of `tests/`, `standalone/` or `.git/`.

The exact inventory covers only files present in the supplied snapshot. Preserve additional local ROM variants, external ZEX images and uncommitted changes before removing old paths. `scripts/`, `Makefile`, `.gitignore`, `README.md` and `README.es.md` are updated in place, not removed.

## Native and browser outputs

Existing `build/host/` executable names remain unchanged. Native programs now default to `common/roms/pacman/` when launched from the repository root. The default can still be overridden through their ROM-directory argument.

This private package includes the browser demo and HTML READMEs, the generated native core/ROM C, and the supplied Pi 4 example under root `build/`. It excludes native test binaries, objects and Git metadata. Rebuild the browser with `make pacman-web`, HTML READMEs with `make readme-pages`, and Raspberry Pi with `make pacman-rpi MODEL=4 TOOLCHAIN=clang`. `make pacman-rpi-example TOOLCHAIN=clang` recreates the prepared Pi 4 directory after cleaning root `build/`.

Raw ROMs, generated ROM C, kernels and browser bundles stay Git-ignored. Version only sources, headers, scripts, configuration, tests and documentation.

## Raspberry Pi package integration

The new package is **complete**, not an overlay ZIP for Raspberry Pi only. Hardware source paths remain under `standalone/platforms/raspberry_pi/`. The generator is retained at `standalone/tools/generate_pacman.py`; it now reads the shared `common/` C sources and writes to root `build/`. The incoming native runtime is unchanged apart from source-path comments in its generated C.

| Incoming standalone path | Integrated path |
| --- | --- |
| `standalone/pacman.c` | `build/standalone/native/pacman.c` (regenerated, not hand-maintained) |
| `standalone/pacman.h` | `standalone/include/pacman.h` |
| `standalone/platform.h` | `standalone/include/platform.h` (existing equivalent API) |
| Raspberry Pi `roms/pacman/` | `common/roms/pacman/` (identical payloads; one input copy) |
| Raspberry Pi `roms/CHECKSUMS.txt` | `common/roms/CHECKSUMS.txt` |
| Raspberry Pi `generated/roms.c` | `build/standalone/raspberry_pi/generated/roms.c` |
| Raspberry Pi `examples/pi4_usb/{kernel8.img,config.txt,SHA256SUMS}` | `build/standalone/raspberry_pi/examples/pi4_usb/` |
| `standalone/FIRST_RUN_PI4.md` | Merged into the bilingual platform example README |
| `standalone/VALIDATION.txt` | `docs/validation/raspberry-pi-imported.txt` (historical, unmodified) |
| `standalone/MANIFEST.txt` | `docs/migration/raspberry-pi-input-manifest.txt` (original-package inventory) |
| Nested standalone/Raspberry Pi `.gitignore` files | Merged into root `.gitignore` |

The SHA-256 manifest is rebased to the new example directory; kernel/config bytes are unchanged. Ordinary builds do not replace the supplied example. Old `generated/README.md` and `roms/README.md` instructions are merged into current platform/shared-ROM documentation.

### Which removal list applies?

- **Already using the previous reorganized ZIP, and never copied the Raspberry ZIP over it:** no existing files need deletion; copy this package over that root.
- **Already reorganized, but also copied the Raspberry ZIP's old layout:** use [remove-raspberry-pi-old-paths.txt](remove-raspberry-pi-old-paths.txt) and [its exact map](raspberry-pi-path-map.tsv).
- **Still using the original project layout:** use the full [remove-old-paths.txt](remove-old-paths.txt), which now covers both source packages. Missing old paths can simply be skipped.

These are individual file lists, not shell scripts. Copy the new package first, verify destinations, and remove only listed obsolete files that actually exist. `.git/` and any local-only files must be preserved. Existing platform-local `build/` output was not in the incoming ZIP and is not automatically deleted; inspect it separately. Remove empty directories only after checking their contents. Neither removal list contains a current project file.
