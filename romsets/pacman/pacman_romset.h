#ifndef ARCADE_CORE_PACMAN_ROMSET_H
#define ARCADE_CORE_PACMAN_ROMSET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    PACMAN_PROGRAM_ROM_CHIP_SIZE = 0x1000,
    PACMAN_PROGRAM_ROM_CHIP_COUNT = 4,

    PACMAN_ROMSET_PROGRAM_SIZE =
        PACMAN_PROGRAM_ROM_CHIP_SIZE *
        PACMAN_PROGRAM_ROM_CHIP_COUNT,

    PACMAN_ROMSET_TILE_ROM_SIZE = 0x1000,
    PACMAN_ROMSET_SPRITE_ROM_SIZE = 0x1000,

    PACMAN_ROMSET_PALETTE_PROM_SIZE = 0x0020,
    PACMAN_ROMSET_COLOR_LOOKUP_PROM_SIZE = 0x0100,

    PACMAN_ROMSET_SOUND_WAVEFORM_PROM_SIZE = 0x0100,
    PACMAN_ROMSET_SOUND_TIMING_PROM_SIZE = 0x0100
};

typedef struct {
    /*
     * CPU-visible linear image:
     *
     *   6E -> 0000-0FFF
     *   6F -> 1000-1FFF
     *   6H -> 2000-2FFF
     *   6J -> 3000-3FFF
     */
    uint8_t program[PACMAN_ROMSET_PROGRAM_SIZE];

    /*
     * Video-board ROMs/PROMs.
     */
    uint8_t tile_rom[PACMAN_ROMSET_TILE_ROM_SIZE];
    uint8_t sprite_rom[PACMAN_ROMSET_SPRITE_ROM_SIZE];

    uint8_t palette_prom[PACMAN_ROMSET_PALETTE_PROM_SIZE];

    uint8_t color_lookup_prom[
        PACMAN_ROMSET_COLOR_LOOKUP_PROM_SIZE
    ];

    /*
     * Sound-board PROMs.
     *
     * 82s126.1m contains the 8 x 32-sample Namco WSG waveforms.
     * 82s126.3m is retained as part of the physical ROM set even
     * though the first host sound implementation does not consume it.
     */
    uint8_t sound_waveform_prom[
        PACMAN_ROMSET_SOUND_WAVEFORM_PROM_SIZE
    ];

    uint8_t sound_timing_prom[
        PACMAN_ROMSET_SOUND_TIMING_PROM_SIZE
    ];
} PacmanRomSet;

bool pacman_romset_build_program(
    PacmanRomSet *romset,

    const uint8_t *chip0,
    size_t chip0_size,

    const uint8_t *chip1,
    size_t chip1_size,

    const uint8_t *chip2,
    size_t chip2_size,

    const uint8_t *chip3,
    size_t chip3_size
);

bool pacman_romset_set_sound(
    PacmanRomSet *romset,

    const uint8_t *waveform_prom,
    size_t waveform_prom_size,

    const uint8_t *timing_prom,
    size_t timing_prom_size
);


bool pacman_romset_set_video(
    PacmanRomSet *romset,

    const uint8_t *tile_rom,
    size_t tile_rom_size,

    const uint8_t *sprite_rom,
    size_t sprite_rom_size,

    const uint8_t *palette_prom,
    size_t palette_prom_size,

    const uint8_t *color_lookup_prom,
    size_t color_lookup_prom_size
);

#endif
