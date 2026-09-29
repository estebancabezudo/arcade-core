# Gowin wrappers

Future Gowin-specific PLL, SDRAM and TMDS implementations belong here. Each
wrapper must present a generic interface to the rest of the design so that
`rtl/` and the Tang Nano logical shell remain Verilator-simulable.

Do not add generated IP or guessed primitive parameters until the exact board
clock, device/package, SDRAM and HDMI wiring are verified.
