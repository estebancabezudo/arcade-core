# Raspberry Pi bare-metal Pac-Man

[Project overview](../../../README.md) | [Español](README.es.md) | [First boot](examples/pi4_usb/README.md)

This is the implementation imported from the supplied standalone Raspberry Pi package, not an empty platform placeholder. It runs the generated native C core as an AArch64 kernel without Linux, SDL, MAME or the RTL/CRT pipeline. The primary target is **Raspberry Pi 4 Model B**; `MODEL=3` retains the incoming Pi 3/Zero 2 build configuration. Pi 5 is not implemented.

**Physical validation is still pending.** This integration compiles the Pi 3 and Pi 4 configurations with Clang 17/LLD and preserves the supplied Pi 4 image. Compilation and host emulation tests do not verify firmware interaction, boot, GPIO, video or audio on a real board.

## Layout

```text
standalone/platforms/raspberry_pi/
├── Makefile
├── boot/config.txt
├── boot/start.S
├── include/raspberry_pi.h
├── linker/kernel8.ld
├── src/main.c
├── src/platform.c
├── scripts/
│   ├── pack_roms.py
│   ├── install_boot.sh
│   ├── install_pi4_example.sh
│   └── install_image.sh
└── examples/pi4_usb/             # documentation; image is under root build/
```

The public native headers are in `standalone/include/`; the generator is `standalone/tools/generate_pacman.py`. ROM input is centralized in `common/roms/pacman/`. All generated files live under the **repository root** `build/`, never under this platform's source directory.

## Prepared Pi 4 example

The private distribution includes, relative to the repository root:

```text
build/standalone/raspberry_pi/examples/pi4_usb/
├── kernel8.img
├── config.txt
└── SHA256SUMS
```

This is the original supplied image and configuration. No rebuild is needed for the initial hardware test. Follow the [first-boot guide](examples/pi4_usb/README.md); the firmware and a suitable boot medium must be provided separately.

## Build from the repository root

With the AArch64 GCC/binutils cross-toolchain, Python 3 and GNU Make installed:

```bash
make pacman-rpi MODEL=4 TOOLCHAIN=gcc
```

Alternatively, with Clang/LLVM, LLD and `llvm-objcopy` installed:

```bash
make pacman-rpi MODEL=4 TOOLCHAIN=clang
```

The imported package targets Clang 17 or later; this integration was compiled with Clang 17. GCC configuration is retained, but GCC cross-compilation was not exercised here. An alternate GCC prefix can be supplied with `CROSS_COMPILE=...`.

Output paths distinguish both board and toolchain, preventing Pi 3 objects from being reused for Pi 4:

```text
build/standalone/native/pacman.c
build/standalone/raspberry_pi/generated/roms.c
build/standalone/raspberry_pi/model4/gcc/kernel8.img
build/standalone/raspberry_pi/model4/clang/kernel8.img
```

Only the selected toolchain directory is built. `MODEL=3` selects Cortex-A53 and its peripheral base; `MODEL=4` selects Cortex-A72 and its peripheral base. The build retains `-O3`, LTO, freestanding flags and warnings as errors. It neither downloads firmware nor boots hardware.

The equivalent platform-local invocation is `make -C standalone/platforms/raspberry_pi MODEL=4 TOOLCHAIN=clang` from the repository root.

## Generators and clean builds

`make pacman-standalone` regenerates the core. `make pacman-rpi-roms` validates sizes and CRC32 of the shared ten-ROM set and generates the C payload. `make pacman-rpi` invokes both as needed; generators preserve output timestamps when bytes are unchanged. A different local ROM directory can be passed as `ROM_DIR=/absolute/path` to `make pacman-rpi`.

The platform's `clean` target removes only the selected model/toolchain directory. Removing root `build/` also removes the prepared image and generated ROM/core C. Rebuild the kernel with `make pacman-rpi`, and explicitly recreate the ready-to-copy Pi 4 example with:

```bash
make pacman-rpi-example TOOLCHAIN=clang
```

This last command replaces the prepared example with the freshly built Pi 4 kernel and regenerates `SHA256SUMS`. Ordinary compilation does **not** replace the supplied example.

## Installation

From the repository root, after mounting an existing Raspberry Pi boot partition:

```bash
bash standalone/platforms/raspberry_pi/scripts/install_pi4_example.sh /path/to/boot
```

For a kernel built locally with the Pi 4 Clang configuration:

```bash
bash standalone/platforms/raspberry_pi/scripts/install_boot.sh /path/to/boot 4 clang
```

The last two arguments default to `4 gcc`; choose the actual build. Both installers replace **only** `kernel8.img` and `config.txt`, save previous copies under `arcade-core-backup/install.*`, and leave other firmware files untouched. The example installer also checks the supplied SHA-256 manifest. They do not format, mount or provision a device, install firmware, or change EEPROM boot order. Use a dedicated test medium and keep any boot customizations you need.

## Implemented hardware backend

| Area | Incoming implementation |
| --- | --- |
| Boot | AArch64 entry at `kernel8.img`; secondary cores parked; `.bss` cleared |
| Video | Firmware-mailbox framebuffer request, 1280×720×32; 224×288 RGB565 content scaled 2× to 448×576 and centered |
| Inputs | Active-low GPIO buttons with external 3.3 V pull-ups |
| Audio | Mono PCM16 at 48 kHz converted to synchronous software PDM on GPIO18 |
| Timing | Generic AArch64 counter; polled execution |
| ROMs | Embedded at build time; no runtime filesystem loading |

GPIO numbers below are the identifiers used in `include/raspberry_pi.h`, **not physical connector pin numbers**:

| Function | GPIO |
| --- | ---: |
| Up | 5 |
| Down | 6 |
| Left | 13 |
| Right | 19 |
| Coin | 16 |
| Start 1 | 20 |
| Start 2 | 21 |

Buttons connect between GPIO and GND and require external pull-ups to 3.3 V. Do not connect 5 V to GPIO. GPIO18 requires an appropriate low-pass filter and amplified input; never connect a speaker directly. The imported first-run guide describes its initial test circuit, which remains unvalidated on hardware here.

## Limitations

USB can be the boot medium, but the kernel has **no USB keyboard/joystick driver, FAT/USB filesystem, runtime ROM loading, HDMI audio, built-in analogue-jack audio, DMA/PWM audio backend or Pi 5 support**. The software-PDM write is synchronous and can affect latency and effective speed; it is a bring-up backend, not a validated low-latency audio solution.

The common core and native wrapper are independent of this hardware implementation. The browser demonstration is also independent and does not validate Raspberry Pi. See [integration checks](../../../docs/validation/raspberry-pi-integration.md).
