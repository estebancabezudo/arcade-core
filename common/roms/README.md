# Local ROM input

Supply the ten Pac-Man ROM/PROM files listed in the [root README](../../README.md#pac-man-roms) under `common/roms/pacman/`. This private distribution preserves the files supplied by the user; Git ignores them. The native loader and JavaScript packager use this same input directory. The ROM-free browser template is versionable; the generated JavaScript contains ROM bytes and stays under ignored `build/`.
