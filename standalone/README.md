# Standalone targets

[Project overview](../README.md) | [Español](README.es.md)

Standalone targets package a complete Pac-Man runtime without SDL, MAME or the FPGA/CRT pipeline. The browser still requires a browser and its operating system; Raspberry Pi runs a native AArch64 kernel without Linux.

## Shared native layer

`include/pacman.h` defines the ROM regions and `pacman_standalone_init()`, `pacman_standalone_reset()`, `pacman_standalone_frame()` and `pacman_standalone_run()`. `include/platform.h` supplies the hardware boundary:

```c
bool platform_init(void);
void platform_video_present(const uint16_t *pixels, size_t pixel_count);
void platform_audio_write(const int16_t *samples, size_t sample_count);
PacmanStandaloneInput platform_input_read(void);
uint64_t platform_time_us(void);
void platform_idle(void);
```

The core outputs RGB565 at 224×288 in display orientation and mono PCM16 at 48 kHz. It accepts a four-way joystick, coin and player-start inputs. It does not use a filesystem, `malloc`, threads or an operating system.

From the repository root:

```bash
make pacman-standalone
```

`tools/generate_pacman.py` reads the canonical sources in `common/devices/`, `common/boards/` and `common/machines/`, then writes `build/standalone/native/pacman.c`. Its wrapper preserves the imported standalone implementation, including four-way input selection, a two-frame coin pulse, RGB565 rotation, 4:1 audio decimation and the 16,500 µs frame deadline. A hardware reset retains SRAM as specified by the common CPU board; it is not the same as cold initialization.

The common C sources remain the source of truth. Do not edit generated `pacman.c`; change the common components or the wrapper in the generator and regenerate. The single translation unit permits cross-component compiler optimization. Raspberry Pi uses `-O3`, LTO, freestanding compilation and model-specific CPU flags; this is not a universal performance guarantee.

## Platforms

| Platform | Sources | Runtime and status |
| --- | --- | --- |
| Browser | [platforms/browser/](platforms/browser/README.md) | Implemented JavaScript port; one generated JS file and a canvas-only HTML page |
| Raspberry Pi | [platforms/raspberry_pi/](platforms/raspberry_pi/README.md) | Imported bare-metal backend, Pi 4 prebuilt image and Pi 3/Zero 2 build option; physical validation pending |

The browser is a separate JavaScript port, not a compilation of the C core to WebAssembly. Its ROM packager and the native targets consume the same local `common/roms/pacman/` files.

## Generated outputs

```text
build/standalone/
├── native/pacman.c
├── browser/index.html
├── browser/pacman.js
└── raspberry_pi/
    ├── generated/roms.c
    ├── model4/clang/kernel8.img     # example build selection
    └── examples/pi4_usb/           # prepared Pi 4 image, config and checksums
```

`make pacman-web` builds the browser. `make pacman-rpi MODEL=4 TOOLCHAIN=clang` builds the native kernel. The supplied Pi 4 image is available without recompiling; see the [first-run guide](platforms/raspberry_pi/examples/pi4_usb/README.md).

The private ZIP preserves the supplied ROMs and prepared artifacts. Raw ROMs, generated C with embedded ROMs, kernels and browser bundles remain outside version control under ignored input/output paths. The platform source directories contain no duplicated ROM set.

## Verification

`make test-standalone` runs the native host trace plus generator, ROM packager and installer tests. `make test-web` checks the independent JavaScript path. These are separate from the modular C/RTL `make test` target. See [native test details](../tests/standalone/native/README.md) and [integration validation](../docs/validation/raspberry-pi-integration.md). Compiling an AArch64 kernel does not establish working video, GPIO or audio on physical hardware.
