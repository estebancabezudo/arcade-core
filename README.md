# arcade-core

[English](README.md) | [Español](README.es.md)

`arcade-core` began as a classic hardware emulator designed to run bare-metal on very small devices. Its original architecture is modular, separating the arcade CPU board, audio, and video so it can be extended more easily to other arcade machines. The goal is to avoid booting an operating system or running MAME and instead go directly into the game, as if it were the original machine. It was conceived for a hybrid CPU + FPGA implementation driving HDMI output.

The project is written primarily in:

- C17 for CPU, board, and machine emulation, plus host tools.
- SystemVerilog for the video pipeline.
- C++17 for Verilator testbenches and RTL/SDL integration.

It currently includes a functional **Pac-Man** implementation with:

- Z80 CPU;
- Pac-Man CPU board;
- video, tilemap, sprites, and palette;
- three-voice Namco WSG audio;
- machine execution synchronized by T-states;
- interactive SDL simulator;
- framebuffer and 720p video pipeline in RTL;
- Pac-Man → RTL → SDL integration through Verilator.

The project is not intended to depend on MAME at runtime. The machine components are implemented inside `arcade-core`.

---

## Current status

The current implementation has validated:

- Z80 core through unit tests and ZEXDOC/ZEXALL;
- Pac-Man CPU board decoder and memory map;
- interrupts and VBlank;
- tilemap and sprites;
- Pac-Man raster;
- CPU, video, and sound ROM set;
- three-voice Namco WSG;
- Pac-Man music and sound effects;
- CPU → video → audio synchronization;
- playable SDL simulator;
- RTL framebuffer;
- clock-domain crossing;
- multiple-framebuffer management;
- integer scaling;
- 1280×720 scanout;
- CRT effects;
- Pac-Man → `video_system` RTL integration.

# Project structure

```text
arcade-core/
├── devices/
│   ├── z80/
│   └── namco_wsg/
│
├── boards/
│   ├── pacman_cpu/
│   ├── pacman_video/
│   └── pacman_audio/
│
├── machines/
│   └── pacman/
│
├── romsets/
│   └── pacman/
│
├── platform/
│   └── host/
│
├── rtl/
├── sim/
├── fpga/
│   └── tangnano20k/
│
├── tests/
│   ├── host/
│   ├── namco_wsg/
│   ├── pacman_audio/
│   ├── pacman_cpu/
│   ├── pacman/
│   └── z80/
│
├── scripts/
├── roms/
│   └── pacman/
│
├── Makefile
└── README.md
```

## Layers

### `devices/`

Reusable components that represent specific hardware devices.

Currently:

- `z80`: Zilog Z80 emulator.
- `namco_wsg`: three-voice Namco sound generator used by Pac-Man.

### `boards/`

Implementations of boards or machine subsystems.

Pac-Man is divided into:

```text
pacman_cpu
pacman_video
pacman_audio
```

The CPU board connects the Z80 to memory, I/O, interrupts, and hardware registers.

The video board implements raster timing, tilemap, sprites, color, and timing.

The audio board connects Pac-Man's sound registers to the Namco WSG.

### `machines/`

Integrates the boards that make up a complete machine.

Currently:

```text
machines/pacman/
```

`PacmanMachine` coordinates CPU, video, audio, inputs, and emulation timing.

### `romsets/`

Describes the logical composition of a machine's ROMs independently of how they are loaded from the host.

### `platform/host/`

Code that depends on the host computer:

- file loading;
- ROM loading;
- SDL simulators;
- integration with the simulated RTL pipeline.

### `rtl/`

Generic video pipeline written in SystemVerilog.

### `sim/`

C++ testbenches for Verilator and RTL + SDL integration.

### `fpga/`

Board-specific code for future FPGA targets.

There is currently a logical boundary for the Tang Nano 20K, but no completed physical bitstream.

---

# Requirements

On Debian or derivatives, at least the following packages are required:

```bash
sudo apt install \
    build-essential \
    make \
    pkg-config \
    libsdl2-dev \
    verilator
```

Tools used:

- C compiler with C17 support;
- C++ compiler;
- GNU Make;
- Bash;
- SDL2;
- Verilator.

# Pac-Man ROMs

Commercial ROMs are **not included in the repository**.

To run Pac-Man, place them locally in:

```text
roms/pacman/
```

The following ten files are expected:

```text
pacman.6e
pacman.6f
pacman.6h
pacman.6j
pacman.5e
pacman.5f
82s123.7f
82s126.4a
82s126.1m
82s126.3m
```

The final two belong to the sound subsystem:

```text
82s126.1m    waveform PROM
82s126.3m    sound timing PROM
```

The loader requires every file to have exactly the expected size.

---

# Building Pac-Man

To build the host targets:

```bash
make pacman-host
```

This generates:

```text
build/host/pacman-sim
build/host/pacman-hdmi-sim
build/host/pacman-video-system-sdl
```

The following targets are also available:

```bash
make pacman-sim
make pacman-hdmi-sim
make pacman-video-system
```

These targets currently use the same host build process.

---

# Pac-Man simulators

## `pacman-sim`

This is the main simulator and the functional reference for the machine.

Run:

```bash
./build/host/pacman-sim
```

A different ROM directory can also be specified:

```bash
./build/host/pacman-sim /path/to/roms
```

It includes:

- CPU;
- video;
- audio;
- inputs;
- SDL window;
- real-time execution.

This is the appropriate option for playing and validating Pac-Man's overall behavior.

---

## `pacman-hdmi-sim`

Host viewer that displays Pac-Man inside a 1280×720 surface using the final geometry intended for HDMI.

```bash
./build/host/pacman-hdmi-sim
```

It does not simulate the complete RTL pipeline cycle by cycle.

It is useful for quickly validating orientation, scaling, and presentation.

---

## `pacman-video-system-sdl`

Integrates the Pac-Man machine with the actual SystemVerilog `video_system` through Verilator.

The conceptual path is:

```text
PacmanMachine
      │
      ▼
RGB565 224×288
      │
      ▼
video_system RTL
      │
      ├── capture
      ├── framebuffer
      ├── memory arbitration
      ├── line fetch
      ├── integer scaling
      ├── CRT processing
      ▼
1280×720
      │
      ▼
SDL
```

Run:

```bash
./build/host/pacman-video-system-sdl
```

This program is intended primarily as an RTL integration test, not as the most efficient simulator for playing.

---

# Controls

In the interactive simulators:

| Key        | Function                   |
| ---------- | -------------------------- |
| Arrow keys | Four-way joystick          |
| `5`        | Insert coin                |
| `1`        | Start player 1             |
| `2`        | Start player 2             |
| `F`        | Full screen                |
| `Esc`      | Exit                       |

The original joystick is four-way. The host avoids sending simultaneous diagonal inputs to the emulated machine.

---

# Z80 CPU

The core is located in:

```text
devices/z80/
```

It is designed as an isolated, reusable component and does not depend on any specific game implementation.

The tests cover, among other areas:

- loads;
- 8-bit and 16-bit ALU operations;
- flags;
- jumps;
- stack;
- CB;
- ED;
- DD/FD;
- DDCB/FDCB;
- IX/IY;
- block transfer;
- block I/O;
- interrupts;
- interrupt modes;
- `DAA`;
- `RRD` / `RLD`;
- instructions with ignored prefixes.

In addition to the local test suite, the core has been validated with **ZEXDOC** and **ZEXALL**.

---

# Running ZEXDOC / ZEXALL

`tests/z80/zex_runner.c` is a separate runner because the external `.cim` images are not part of the repository.

Compile:

```bash
mkdir -p build/z80

cc \
    -std=c17 \
    -O2 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Werror \
    -Idevices/z80 \
    devices/z80/z80.c \
    tests/z80/zex_runner.c \
    -o build/z80/zex_runner
```

Run, for example:

```bash
./build/z80/zex_runner /path/to/zexdoc.cim
```

or:

```bash
./build/z80/zex_runner /path/to/zexall.cim
```

ZEX is not run automatically by `make test` because it depends on external files.

---

# Pac-Man video

The original machine is modeled with a raster of:

```text
384 × 264
```

The visible area used by Pac-Man is:

```text
224 × 288
```

The video implementation includes:

- tile RAM;
- tile graphics ROM;
- sprite graphics ROM;
- palette PROM;
- color lookup PROM;
- flip screen;
- sprites;
- transparency;
- VBlank.

The CPU and raster are synchronized using Z80 T-states.

---

# Pac-Man audio

The audio subsystem uses a three-voice Namco WSG device.

Implementation:

```text
devices/namco_wsg/
boards/pacman_audio/
```

Main characteristics:

- 3 voices;
- 8 waveforms;
- 32 samples per waveform;
- 4-bit waveform PROM;
- phase accumulators;
- independent frequency per voice;
- volume from 0 to 15;
- internal generation at 192 kHz.

The relationship used with the Z80 is:

```text
CPU: 3.072 MHz
WSG: 192 kHz
```

so one internal sample is generated every:

```text
16 T-states
```

SDL output is reduced to 48 kHz through 4:1 decimation.

Writes to WSG registers are applied after generating the audio corresponding to the current instruction interval. This prevents a write from retroactively changing time already consumed by that instruction.

This synchronization is important for fast effects such as Pac-Man's eating sound.

---

# RTL video pipeline

The final pipeline starts at:

```text
rtl/video_system.sv
```

The design fully decouples:

```text
source raster
    │
    ▼
capture
    │
    ▼
framebuffer
    │
    ▼
memory
    │
    ▼
HDMI scanout
```

The arcade source never receives backpressure.

If memory cannot accept a complete capture, the frame may be aborted without stopping the original raster.

---

## Framebuffers

The design uses four RGB565 slots.

Conceptual states:

```text
FREE
RESERVED
COMPLETE
READING
```

The events:

```text
COMMIT
ABORT
```

act as barriers with respect to preceding memory writes.

HDMI selects only complete frames.

The presentation policy is:

```text
latest complete frame wins
```

Therefore, if the source runs slightly faster than HDMI, complete frames may be skipped without changing the original machine clock.

---

# Supported geometries

The RTL is parameterizable.

## Generic geometry

```text
320×240
SCALE=3
```

Result:

```text
960×720
```

horizontally centered within:

```text
1280×720
```

## Pac-Man

```text
224×288
SCALE=2
```

Result:

```text
448×576
```

centered within:

```text
1280×720
```

Pac-Man's clock is not modified to convert its approximately 60.606 Hz to 60 Hz.

---

# CRT processing

The pipeline includes discrete, synthesizable CRT effects:

1. vertical beam modulation;
2. horizontal bleed;
3. RGB phosphor mask.

They are applied after scaling and before final output.

No additional framebuffer is used for the CRT effect.

---

# Memory and clock domains

The design handles independent domains for:

- source raster;
- memory;
- HDMI.

Asynchronous FIFOs are used for transfers between clock domains.

The pipeline preserves:

- command ordering;
- `COMMIT` and `ABORT` integrity;
- slot identification;
- frame identification;
- read priority for scanout;
- no stalls on the source raster.

---

# Verification

The main validation is run with:

```bash
make test
```

This command currently performs:

1. complete SystemVerilog lint;
2. RTL tests through Verilator;
3. C device tests;
4. board tests;
5. machine tests;
6. host loader tests;
7. Z80 core tests.

Generated artifacts are stored exclusively under:

```text
build/
```

---

## RTL lint

```bash
make lint
```

Checks:

- generic RTL;
- Tang Nano 20K logical shell.

---

## Geometry and video

```bash
make geometry
```

Runs geometry tests for:

- 320×240;
- Pac-Man 224×288.

The video system test can also be run by itself:

```bash
make video-system
```

---

## C test suite

`make test` automatically compiles and runs files matching:

```text
tests/**/*_test.c
```

The suite currently covers:

- file loader;
- Pac-Man ROM loader;
- Namco WSG;
- audio board;
- CPU board;
- ROM set;
- video;
- sprites;
- tilemap;
- Pac-Man machine;
- video synchronization;
- audio integration;
- Z80 core.

`zex_runner.c` is not part of this suite because it requires an external ZEX image.

---

# Build artifacts

Everything generated must remain under:

```text
build/
```

The directory can be removed completely at any time:

```bash
rm -rf build
```

and rebuilt by running the `make` targets again.

`build/` is not tracked in Git.

---

# Tang Nano 20K

The directory:

```text
fpga/tangnano20k/
```

separates board-specific code from generic RTL.

It currently contains a logical boundary for:

- raster input from a future RP2350/RP2354;
- connection to `video_system`;
- abstract memory interface;
- RGB/sync output for a future TMDS layer.

The project **does not yet claim to work on a physical Tang Nano 20K**.

Before completing the real target, the following must be verified against official documentation for the exact board revision:

- oscillator and clock pin;
- FPGA package and speed grade;
- installed SDRAM;
- SDRAM pins and timing;
- HDMI/TMDS pins;
- required Gowin primitives;
- PLL configuration;
- constraints.

Constraints and electrical parameters must not be invented without this information.

Target-specific status is also documented in:

```text
fpga/tangnano20k/README.md
```

---

# Recommended development workflow

After modifying C:

```bash
make test
```

After modifying RTL:

```bash
make lint
make test
```

To test Pac-Man:

```bash
make pacman-host
./build/host/pacman-sim
```

To validate the complete RTL integration:

```bash
./build/host/pacman-video-system-sdl
```

---

# Design principles

The project aims to maintain these rules:

- devices independent of specific machines whenever possible;
- boards as clear hardware boundaries;
- machines as compositions of boards;
- emulation timing derived from hardware rather than the host;
- no `malloc` inside the emulated machine;
- source raster without backpressure;
- ROM loading isolated in the host platform;
- generic RTL separated from board-specific adapters;
- deterministic tests;
- generated artifacts only under `build/`;
- commercial ROMs kept outside the repository.

---

# Physical target status

The architecture and simulation are fairly advanced, but the physical FPGA implementation remains future work.

In particular, the following are still missing:

- physical SDRAM controller;
- board PLL;
- verified constraints;
- physical TMDS/HDMI;
- synthesis;
- timing closure;
- bitstream;
- testing on real hardware.

Until those stages are complete, RTL results are considered validated in simulation, not on physical FPGA hardware.
