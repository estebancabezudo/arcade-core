# arcade-core

[English](README.md) | [Español](README.es.md)

`arcade-core` began as a classic hardware emulator designed to run bare-metal on very small devices. Its original architecture is modular, separating the arcade CPU board, audio, and video so it can be extended more easily to other arcade machines. The goal is to avoid booting an operating system or running MAME and instead go directly into the game, as if it were the original machine. It was conceived for a hybrid CPU + FPGA implementation driving HDMI output.

The current scope includes modular native/FPGA integrations and a self-contained browser distribution. Bare-metal hardware targets remain separate from the implemented host and browser paths.

The project is written primarily in:

- C17 for CPU, board, and machine emulation, plus host tools.
- SystemVerilog for the video pipeline.
- C++17 for Verilator testbenches and RTL/SDL integration.
- JavaScript for the standalone browser example.

It currently includes a functional **Pac-Man** implementation with:

- Z80 CPU;
- Pac-Man CPU board;
- video, tilemap, sprites, and palette;
- three-voice Namco WSG audio;
- machine execution synchronized by T-states;
- interactive SDL simulator;
- standalone JavaScript browser example;
- framebuffer and 720p video pipeline in RTL;
- Pac-Man → RTL → SDL integration through Verilator.

The project is not intended to depend on MAME at runtime. The machine components are implemented inside `arcade-core`.

For the web version, see the [JavaScript browser example](#javascript-browser-example).

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

The top-level split distinguishes **shared components**, **modular platform integrations**, and **standalone distributions**. Standalone means self-contained distribution; the browser version still runs inside a browser, not on bare metal.

```text
arcade-core/
├── common/
│   ├── devices/                 # z80, namco_wsg
│   ├── boards/                  # pacman_cpu, pacman_video, pacman_audio
│   ├── machines/pacman/
│   ├── romsets/pacman/
│   ├── rtl/                     # reusable video pipeline
│   └── roms/pacman/             # local/private ROM input
├── modular/
│   └── platforms/
│       ├── host/                # SDL, loaders, Verilator + SDL
│       └── fpga/tangnano20k/     # logical shell; physical target pending
├── standalone/
│   ├── include/platform.h      # native platform contract
│   └── platforms/
│       ├── browser/
│       │   ├── index.html
│       │   └── pacman.template.js
│       └── raspberry_pi/        # planned native target, not implemented
├── tests/
│   ├── common/                  # C devices, boards, machine, ROM set
│   ├── modular/host/            # native file/ROM loaders
│   ├── rtl/                     # Verilator testbenches
│   └── standalone/browser/     # JavaScript, packaging, browser smoke
├── scripts/                     # build and verification entry points
├── docs/                        # preview, migration and validation notes
├── build/                       # generated; ignored by Git
│   ├── host/                    # native executables after compilation
│   ├── standalone/browser/     # ready-to-run index.html + pacman.js
│   └── docs/                    # interactive HTML versions of the READMEs
├── Makefile
├── README.md
└── README.es.md
```

## Shared components: `common/`

`devices/` contains the reusable Z80 and Namco WSG devices. `boards/` connects those devices to Pac-Man's CPU, video and audio subsystems. `machines/pacman/` composes the boards as `PacmanMachine`, coordinating inputs and T-state timing. `romsets/pacman/` defines the machine's logical ROM regions independently of file I/O.

`common/rtl/` contains the generic SystemVerilog video pipeline. It has no Tang Nano pin constraints or physical board adapters. `common/roms/pacman/` is the **local, private input** shared by the native loader and JavaScript packager; raw ROMs are ignored by Git.

The browser source is a separate JavaScript port. It does **not** import C or execute the RTL. Sharing ROM input does not imply that edits to the C engine update the JavaScript automatically. The reusable C components can also support a future native standalone port; that integration is not implemented yet.

## Modular integrations: `modular/platforms/`

`host/` owns host file loading, Pac-Man ROM loading, `pacman_sim.c`, `pacman_hdmi_sim.c`, and `pacman_video_system_sdl.cpp`. The last file is an executable host adapter, so it no longer lives among testbenches.

`fpga/tangnano20k/` owns only board-specific integration: the logical shell, RP2350 raster ingress and documentation for future constraints, memory, clocks and TMDS. There is still no verified physical bitstream.

## Standalone targets: `standalone/`

`platforms/browser/` contains the ROM-free JavaScript source template and the canvas-only entry page. `make pacman-web` produces a single self-contained JavaScript file plus its HTML page in `build/standalone/browser/`.

`include/platform.h` is the native standalone platform contract for video, audio, input, time and initialization. `platforms/raspberry_pi/` is explicitly a **planned target**: its existing boot/include/src/linker/scripts directories contain no implementation. Neither a Raspberry Pi boot image nor a native standalone Pac-Man executable is claimed.

## Verification and generated files

Tests mirror their scope under `tests/common/`, `tests/modular/`, `tests/rtl/` and `tests/standalone/`. Shared native source/include lists are centralized in `scripts/common-build.sh`; existing root Make targets continue to work.

Compiled programs, temporary tests, the browser bundle and HTML documentation all live under `build/`. Documentation and migration inventories live under `docs/`.

See [migration instructions](docs/migration/README.md) for the old-to-new path map and the exact obsolete files to remove after copying this distribution into an existing checkout.

---

# Requirements

The following requirements apply to the native host tools and RTL simulation. The [JavaScript browser example](#javascript-browser-example) runs independently in a browser.

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

The package list above is for native/RTL targets. Playing the prepared browser example needs only a browser. Rebuilding its bundle needs Python 3 and local ROMs; `make test-web` also needs Node.js with `node:test` support (Node.js 18 or later). Rendering HTML READMEs uses the optional dependency in `docs/requirements.txt`.

# Pac-Man ROMs

Commercial ROMs are **not included in the repository**. This private working distribution preserves the ten ROMs supplied with the project under the ignored `common/roms/pacman/` directory. Do not publish them or the generated browser bundle as part of the source repository.

To run the native Pac-Man simulators, place them locally in:

```text
common/roms/pacman/
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

In the native SDL simulators:

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

# JavaScript browser example

[Open the canvas-only game](standalone/platforms/browser/index.html) · [Interactive English README](build/docs/README.html)

<iframe src="standalone/platforms/browser/index.html" title="Pac-Man arcade" width="448" height="576" style="border:0;max-width:100%;aspect-ratio:224/288" allow="autoplay; fullscreen" allowfullscreen></iframe>

<details>
<summary>Static preview for readers that do not allow interactive content</summary>

[![Pac-Man arcade screen](docs/images/pacman.png)](standalone/platforms/browser/index.html)

</details>

The inline example is a real game in an HTML renderer that permits iframes. **GitHub's README view does not execute JavaScript**: its [markup pipeline](https://github.com/github/markup#github-markup) sanitizes active HTML. On GitHub the relative game link opens the HTML source, not a hosted game. For a playable example, open the local page or the generated HTML README. No public deployment or working public demo URL is assumed.

The private distribution includes the generated browser files, so the example is ready to open without a compiler. In a fresh source-only checkout, supply the ROMs and build the bundle first.

## Run and rebuild

Open `standalone/platforms/browser/index.html` or `build/standalone/browser/index.html`. The first is the tracked entry page and points to the generated bundle; the second is the portable distribution. The entire portable game consists of:

```text
build/standalone/browser/
├── index.html
└── pacman.js
```

Once the original boot finishes, **click/tap the arcade screen or press Enter** to insert one coin and start one player. Until then, only the last frame of the original boot/attract sequence is displayed; there is no external start overlay. A click while running does not insert another coin. A click while paused resumes without resetting.

The example shows **only the 224 × 288 arcade canvas**: no title bar, toolbar, external buttons, volume slider, performance counters or game overlay. The original score, credits, maze and messages remain because they are part of the arcade image. On touch devices, drag on the screen to select a direction; release clears the held direction.

Regenerate from `common/roms/pacman/` with Python 3 and Make, from the repository root:

```bash
make pacman-web
```

`scripts/build-pacman-web.py` reads `standalone/platforms/browser/pacman.template.js`, validates all ten local ROMs by exact size and CRC32, injects the Base64 payload and writes the distribution to `build/standalone/browser/`. The template is versionable and contains **no commercial ROM bytes**. The generated bundle and raw ROMs remain ignored by Git. Both preserve the ROMs supplied locally; nothing is downloaded.

You can copy just `build/standalone/browser/index.html` and `pacman.js` to another directory or a static web server. Playing does not require Python, Node.js, C, SDL or Verilator. Open the HTML page, not the JavaScript file. Browser restrictions may affect `file://` pages; serving the same two files through a static server is the alternative.

## HTML integration and UTF-8

Save this example as an HTML file **at the project root** after building. All files must be UTF-8:

```html
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Pac-Man</title>
  <style>
    body { margin: 0; background: #000; }
    #game { width: min(448px, 100vw, calc(100dvh * 224 / 288)); margin: auto; }
  </style>
</head>
<body>
  <div id="game"></div>
  <script>
    window.PacmanOptions = { container: '#game', screenOnly: true, volume: 0.65 };
  </script>
  <script src="./build/standalone/browser/pacman.js" charset="utf-8" defer></script>
</body>
</html>
```

Set `PacmanOptions` before the bundle. `screenOnly: true` selects the canvas-only DOM, rather than hiding a full panel with CSS. Omit it, or set it to `false`, to retain the original complete interface. `volume` accepts a value between 0 and 1. Both modes use the same machine, renderer and audio engine.

Sound is initialized from a user interaction; browser [autoplay rules](https://developer.mozilla.org/en-US/docs/Web/Media/Guides/Autoplay) may otherwise block it. If audio initialization fails, the game can run silently. Hiding the tab pauses the machine. Returning to the tab does not restart it automatically: click the screen or use Enter/P to resume.

## Browser controls

| Input | Action |
| --- | --- |
| Click/tap screen or Enter | First start; later resume a pause |
| Arrow keys / WASD | Four-way joystick |
| Touch drag | Select one direction; release clears it |
| `5`, `C`, Space | Insert coin |
| `1` / `2` | Start one / two players with available credits |
| `P` / `Esc` | Pause/resume; Esc does not exit as in SDL |
| `M` | Mute/unmute |
| `F` | Full screen when available |

During a running game, Enter sends the one-player start input; a click only focuses the canvas. On-screen controls belong only to the optional full-interface mode. Reset is available through `PacmanWeb.reset()` in canvas-only mode.

## How it works

1. **ROMs:** `decodeROMs()` decodes the injected Base64 block and rechecks `ROM_MANIFEST` sizes and CRC32. CPU, graphics, palette and sound PROMs stay inside the one JavaScript bundle. Base64 is encoding, not compression.
2. **CPU and timing:** the Z80 executes the actual ROM instructions. `Machine.step()` advances raster and sound by instruction T-states: 3,072,000 per second, 50,688 per machine frame, approximately 60.606 frames/s. VBlank raises the enabled interrupt. Game rules, enemy behavior and scoring come from the ROM, not newly written JavaScript gameplay logic.
3. **Video:** `Renderer` predecodes tiles, sprites and palettes. Changed background tiles are cached; sprites are composed, orientation applied and a reused `ImageData` is presented to Canvas 2D at 224 × 288 with pixel smoothing disabled.
4. **Audio:** the three-voice Namco WSG produces an internal sample every 16 T-states (192 kHz); four samples are averaged to PCM16 at 48 kHz. Deferred WSG register writes are applied **after** generating the current instruction's audio. The timing PROM is retained and verified; sample generation uses the model's timing rather than reading that PROM.
5. **Playback:** an embedded `AudioWorklet` is loaded through a local Blob. Transferable PCM buffers are recycled into a ring buffer. If worklet setup is unavailable, `AudioBufferSourceNode` scheduling is used. CPU emulation and audio synthesis remain on the main thread; the worklet performs playback.
6. **Synchronization:** `PacmanApp.pump()` uses the audio clock, or `performance.now()` without audio. `requestAnimationFrame` presents the latest completed frame independently of the emulated machine clock. Long stalls are bounded and hidden tabs pause instead of catching up in a burst.

```text
Local ROM files --> build-pacman-web.py --> self-contained pacman.js
                                             |
                                      Z80 + Machine + input
                                             |
                          +------------------+------------------+
                          |                                     |
                  Renderer --> Canvas 2D              Namco WSG --> Web Audio
```

The web version does not execute `common/rtl/`, Verilator, HDMI scanout or the RTL CRT effects. It is a standalone software presentation of the machine, not proof that an FPGA target works.

## JavaScript API

For manual mounting, define `window.PacmanOptions = { autoMount: false }` before loading the bundle, then mount after the target element exists:

```javascript
const game = window.PacmanWeb.mount('#game', { screenOnly: true, volume: 0.65 });
game.ready.then(() => console.log(window.PacmanWeb.getStats()));
```

`game.ready` finishes after the boot attempt; `getStats().booted` indicates success. Start audio/gameplay from a click or key event, not from the ready promise.

| API | Purpose |
| --- | --- |
| `PacmanWeb.mount(target, options)` | Mount one instance; replace the previous one |
| `PacmanWeb.play()` | Insert coin and request a one-player start |
| `PacmanWeb.pause()` / `resume()` | Pause/continue the same game |
| `PacmanWeb.insertCoin()` | Insert coin and request resuming |
| `PacmanWeb.reset()` | Reset and run the original boot again |
| `PacmanWeb.getStats()` | Inspect execution, timing and audio counters |
| `PacmanWeb.destroy()` | Stop, release resources and remove the host |

Use `resume()`, not `play()`, to continue an existing game. Statistics remain available through the API even when the screen-only view has no counters.

## Browser verification and interactive README files

```bash
make test-web
```

This rebuilds the bundle, runs Node.js regression tests and Python packager tests. It does **not** launch a browser. The optional Playwright smoke test is documented in [browser test instructions](tests/standalone/browser/README.md). C/RTL `make test` remains separate and does not require Node.js.

The optimizations are typed arrays, predecoded graphics/palettes, dirty-tile caching, reused framebuffers and PCM recycling on the worklet path. The fallback still allocates audio buffers/nodes. These are concrete optimizations, not a universal performance guarantee.

The private package also contains HTML versions of both READMEs under `build/docs/`; their embedded iframe is playable without a Markdown viewer. To regenerate those pages, install the optional documentation dependency from `docs/requirements.txt` and run `make readme-pages`. That dependency is needed only to regenerate documentation, not to play.

See [validation notes](docs/VALIDATION.md) for checks performed during this reorganization and explicit environment limitations.

---

# Z80 CPU

The core is located in:

```text
common/devices/z80/
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

`tests/common/z80/zex_runner.c` is a separate runner because the external `.cim` images are not part of the repository.

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
    -Icommon/devices/z80 \
    common/devices/z80/z80.c \
    tests/common/z80/zex_runner.c \
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
common/devices/namco_wsg/
common/boards/pacman_audio/
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
common/rtl/video_system.sv
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
tests/common/**/*_test.c
tests/modular/**/*_test.c
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

This also removes the generated browser distribution and HTML READMEs. Recreate the browser with `make pacman-web` and the HTML documentation with `make readme-pages`. The inputs remain in `standalone/platforms/browser/` and `common/roms/pacman/`; cleaning `build/` does not remove them.

`build/` is not tracked in Git.

---

# Tang Nano 20K

The directory:

```text
modular/platforms/fpga/tangnano20k/
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
modular/platforms/fpga/tangnano20k/README.md
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
