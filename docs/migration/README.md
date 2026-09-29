# Directory migration

[Español](README.es.md) | [Project README](../../README.md)

This package is based on the supplied `arcade-core(2).zip`. It has no extra enclosing directory: its `Makefile`, `common/`, `modular/` and `standalone/` belong directly in the existing repository root.

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
| `standalone/platforms/raspberry_pi/` | Unchanged; now explicitly documented as planned |

## Obsolete files to remove

[remove-old-paths.txt](remove-old-paths.txt) lists every old file path to remove, one relative path per line. [path-map.tsv](path-map.tsv) provides the corresponding destination. Remove those old files only after confirming the new copies exist; the JavaScript payload now lives in the generated bundle rather than the ROM-free template.

The obsolete directory roots are `devices/`, `boards/`, `machines/`, `romsets/`, `rtl/`, `roms/pacman/`, `platform/host/`, `sim/`, `fpga/tangnano20k/`, `tests/host/`, `tests/namco_wsg/`, `tests/pacman/`, `tests/pacman_audio/`, `tests/pacman_cpu/`, `tests/z80/`, and `standalone/browser/`, plus the file `standalone/platform.h`. Remove emptied parent directories only after checking for additional local files. Do not recursively delete all of `tests/`, `standalone/` or `.git/`.

The exact inventory covers only files present in the supplied snapshot. Preserve additional local ROM variants, external ZEX images and uncommitted changes before removing old paths. `scripts/`, `Makefile`, `.gitignore`, `README.md` and `README.es.md` are updated in place, not removed.

## Native and browser outputs

Existing `build/host/` executable names remain unchanged. Native programs now default to `common/roms/pacman/` when launched from the repository root. The default can still be overridden through their ROM-directory argument.

This private package includes only the ready-to-run browser and HTML README outputs under `build/`; it includes no native test binaries or old object files. Existing native build artifacts in your checkout can be rebuilt with the unchanged Make targets. After deleting all of `build/`, regenerate the browser with `make pacman-web` and the HTML READMEs with `make readme-pages`.

Raw ROMs and generated bundles remain Git-ignored. Only the browser source template and entry-page source should be versioned. The private ZIP intentionally preserves the user-supplied ROMs so its included browser example remains immediately executable.
