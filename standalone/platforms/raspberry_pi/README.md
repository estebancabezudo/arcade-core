# Raspberry Pi — planned native target

This target is **not implemented**. The original snapshot contains only empty `boot/`, `include/`, `src/`, `linker/` and `scripts/` directories. Their names reserve the intended areas; no bootable image, board model, peripheral mapping, linker layout or hardware result is implied.

The generic native interface is [standalone/include/platform.h](../../include/platform.h). A future implementation must select an exact Raspberry Pi model and verified hardware details before defining clocks, memory maps or boot files. The browser game is independent of this target.
