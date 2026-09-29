# Modular platform integrations

[Project overview](../README.md) | [Español](README.es.md)

`platforms/host/` composes the shared machine with native file loading, SDL presentation and the Verilator/SDL adapter. `platforms/fpga/tangnano20k/` contains the logical physical-board boundary, not a verified bitstream. Shared C models and generic RTL stay in `../common/`.

Run the existing `make pacman-host`, `make lint`, `make geometry` and `make test` commands from the repository root. Native executables retain their `build/host/` paths. The default ROM directory is now `common/roms/pacman/`, relative to the repository root; an explicit ROM directory argument is still supported.
