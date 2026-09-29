# Standalone targets

[Project overview](../README.md) | [Español](README.es.md)

- `platforms/browser/`: implemented JavaScript port. `make pacman-web` generates a one-file runtime with embedded ROMs under `build/standalone/browser/`; its companion HTML shows only the arcade screen.
- `include/platform.h`: C contract for native video, PCM audio, inputs, initialization and a monotonic timer.
- `platforms/raspberry_pi/`: planned native target. No bootloader, linker script, hardware drivers or native standalone game loop is implemented.

The browser bundle is self-contained but requires a browser/OS. It does not implement the native platform contract, compile shared C to WebAssembly, or validate a bare-metal target.
