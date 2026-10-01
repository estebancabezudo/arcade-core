# Native standalone verification

From the repository root:

```bash
make test-standalone
```

Requirements: a host C17 compiler, Python 3, GNU Make and Bash. The local ROMs must be present in `common/roms/pacman/`. No Raspberry Pi board or cross-compiler is needed for this target. Temporary outputs stay in root `build/`.

The C fixture runs 720 frames with a scripted coin/start/movement sequence, then a hardware reset followed by another 720 frames. Each trace emits 570,240 PCM samples and hashes every RGB565 frame and PCM16 sample using byte-defined FNV-1a. `original-trace.txt` was produced with the **incoming** native `pacman.c` and incoming `generated/roms.c`, before integration. The rewritten build is compared against that oracle. Hardware reset deliberately retains SRAM, so cold and reset video hashes differ in the original as well; both references are preserved rather than treating reset as cold initialization.

The fixture also checks rejection of missing/invalid ROM descriptors and frame calls before initialization. It validates the output stream, not physical GPIO/PDM/framebuffer behavior or every internal CPU register.

Eleven Python tests cover deterministic generation, unchanged-output timestamps, missing/wrong-size/corrupted ROMs, invocation outside the checkout, model-specific build paths, rejection of unsupported models, and installer backup/checksum/symlink safeguards. The prepared Pi 4 installer is exercised from a different working directory. Installer tests use temporary ordinary directories, not disks or real boot partitions.

`make test-c` covers the existing shared/modular C suite. `make test-web` covers the independent JavaScript bundle. Physical Raspberry Pi boot and audio/video still require hardware validation; see [integration results](../../../docs/validation/raspberry-pi-integration.md).
