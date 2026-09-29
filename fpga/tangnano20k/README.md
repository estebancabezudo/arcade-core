# Tang Nano 20K integration boundary

This directory keeps board-specific work separate from the generic RTL in
`rtl/`. The current deliverable is a simulation-checked logical shell, not a
claim of synthesis or operation on physical hardware.

## Present boundary

`rtl/tangnano20k_video_shell.sv` connects:

- an RP2350 raster interface with no return backpressure;
- the generic `video_system`;
- the existing abstract 32-bit memory command/response interface;
- a parallel RGB565/active/hsync/vsync output for a future TMDS wrapper.

The memory interface remains compatible with a controller that combines
sequential line reads into bursts of 160 words. Burst coalescing must not
change command order, write completion, or read response order.

## Hardware facts still requiring authoritative confirmation

No board constraints, PLL primitive parameters, SDRAM controller, or TMDS
serializer are supplied yet. The repository contains no authoritative local
pinout or board reference design from which to verify:

- the oscillator frequency and exact clock pin;
- the installed GW2AR-18 package/speed-grade identifier;
- the SDRAM part number, bus width, pins, timing and clock phase;
- HDMI/TMDS pins and the required Gowin serializer primitives.

These data must be taken from the exact Tang Nano 20K revision's schematic,
official constraint file and Gowin device database. Until then, inventing a
`.cst`, PLL divider values or SDRAM timing would create a misleading hardware
target. Once confirmed, place only the board adapters here:

```text
fpga/tangnano20k/
├── constraints/   # verified .cst/.sdc files
├── gowin/         # PLL, SDRAM and TMDS primitive wrappers/IP
├── rtl/           # board shell and RP2350 ingress
└── scripts/       # reproducible Gowin/Apicula build commands
```

## Physical bring-up order

1. Confirm the exact board revision, schematic and official pin constraints.
2. Generate/verify a 74.25 MHz pixel clock and expose a locked-derived reset.
3. Validate SDRAM initialization and isolated 32-bit command/response tests.
4. Run line-fetch tests with 160-word sequential reads and worst-case refresh.
5. Add RGB565-to-TMDS serialization and verify the 1280x720@60 raster.
6. Connect the RP2350 source and verify that forced capture pressure produces
   `ABORT`/frame drops without ever stopping the source raster.
