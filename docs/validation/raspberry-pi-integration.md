# Raspberry Pi integration validation

[Project README](../../README.md) | [README español](../../README.es.md)

## Scope and provenance

This delivery integrates `arcade-core-standalone-pacman-rpi4(2).zip` into the previously reorganized project. It is a complete project distribution, not a Raspberry-only overlay. The original [standalone validation report](raspberry-pi-imported.txt) is retained without editing. Paths in that report and in the [incoming manifest](../migration/raspberry-pi-input-manifest.txt) describe the original package, not current output locations.

The common emulator/RTL source files, native shared/modular tests, browser runtime/template, ten original ROM payloads, Raspberry Pi `boot/start.S`, `boot/config.txt`, `linker/kernel8.ld`, `include/raspberry_pi.h`, `src/main.c` and `src/platform.c` are unchanged. Generated native C is identical to the imported `pacman.c` after normalizing only `BEGIN/END common/` source-path comments. Generated ROM C is byte-identical to the supplied `generated/roms.c`.

Integration changes concern paths, output/dependency handling, centralized ROM input, atomic deterministic generators, CRC validation, installers with backups, documentation and regression fixtures. The existing documented `standalone/include/platform.h` has the same public declarations as the incoming header; no API change was necessary. The native wrapper retains the common core's deliberate SRAM-preserving reset semantics.

## Checks executed for this delivery

- Existing common/modular C suite: **36 executables pass**, C17, `-O2`, `-Wall`, `-Wextra`, `-Wpedantic`, `-Werror` via `make test-c`.
- Existing web regressions: **7 Node.js tests and 5 Python tests pass** via `make test-web`, including the existing 720-frame JavaScript CPU/RAM/video/audio reference.
- New native regression: **720 cold-start frames plus 720 frames after hardware reset** match the incoming standalone implementation. Each pass emits **570,240 PCM16 samples**. Every RGB565 framebuffer and audio sample is included in deterministic byte-defined FNV-1a hashes. Invalid ROM descriptors and frame calls before initialization are also checked.
- `make test-standalone`: the native regression and **11 Python integration tests pass**. They cover generator reproducibility and stable timestamps, corrupt/missing/wrong-size ROM input, alternate working directories, separate model/toolchain build paths, unsupported model rejection, backup behavior, checksum failure, symlink refusal, and installation of the actual prepared example into a temporary directory.
- Bare-metal builds: **Pi 4 / Cortex-A72 and Pi 3 / Cortex-A53 compile with Clang 17 + LLD**, using the relocated source and generated paths. No libc or OS is linked. Model output directories are separate. Rebuilding the same Pi 4 selection without changing inputs does not recompile objects.
- Pi 4 rebuilt image is **byte-identical to the supplied precompiled image**, SHA-256 shown below. The ready-to-copy directory keeps the supplied kernel/config bytes; the checksum filenames are rebased to that directory.

Native trace oracle:

```text
cold frames=720 samples=570240 video=5e19d1eb30909326 audio=78232298db8460a9
reset frames=720 samples=570240 video=3c11784f674a0692 audio=78232298db8460a9
```

Cold and reset video hashes differ in the supplied original because CPU reset deliberately preserves SRAM. The regression compares each with its own original reference, not with an incorrect cold-reset equivalence assumption. This is an output regression, not a new exhaustive test of every CPU register or opcode.

## Browser documentation checks

The GitHub-facing Markdown READMEs now contain a visible static screenshot instead of active iframe markup and misleading "open game" source links. Only `scripts/render-readmes.py` replaces the preview markers with a live iframe pointing to `build/standalone/browser/index.html`. The generated README files are not public hosting; no GitHub Pages deployment or public URL has been created.

- Four documentation regressions pass via `make test-docs`: static Markdown without active script/iframe elements, exactly one live iframe per generated README, correct portable target, explicit UTF-8/language, and deterministic rendering from another working directory.
- Seven existing Chromium browser smoke tests pass in inline-fixture mode.
- Both regenerated HTML READMEs also pass an explicit Chromium iframe fixture: one canvas, no external buttons/panels, successful start, advancing frames and pause. This replaces only iframe/script URL loading with injected local content because this environment blocks local navigation. It does **not** verify direct `file://`, HTTP loading, hosted GitHub Pages, or AudioWorklet module loading.

## Prepared Pi 4 image

Current location:

```text
build/standalone/raspberry_pi/examples/pi4_usb/kernel8.img
```

SHA-256:

```text
80ad2a156cf3861e7d7379aac910f4ac004235bd4f39b9f3c4ca170b35d09b6c
```

The accompanying `config.txt` is unchanged:

```text
463075503c8b1f0e7a5a20a700c37f7a34a5ccdf230065b4f03718d338eb95b7
```

`SHA256SUMS` now refers to `kernel8.img` and `config.txt` in its own directory. This corrects the old manifest's platform-relative paths after relocation without changing the payloads.

## Limits

**No physical Raspberry Pi or FPGA was tested.** Boot firmware/EEPROM behavior, framebuffer mailbox behavior, GPIO electrical/input behavior, GPIO18 PDM timing/audio, latency and effective gameplay speed require real hardware. The source backend is implemented and cross-compiles; it must not be documented as physically validated. USB input, a runtime filesystem, HDMI audio, DMA/PWM audio and Pi 5 support have not been added.

AArch64 GCC cross-tools, SDL development headers and Verilator are unavailable in this runner. GCC cross-builds, the complete SDL/RTL integration and FPGA hardware checks were not executed. Their retained configuration is not a new validation claim.

The historical report's 1,800-frame smoke and original compile claims are preserved as supplied provenance. The new regression uses the explicitly documented two 720-frame traces instead. No universal optimization or cross-browser performance claim is made.
