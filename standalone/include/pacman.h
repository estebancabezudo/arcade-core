#ifndef ARCADE_CORE_STANDALONE_PACMAN_H
#define ARCADE_CORE_STANDALONE_PACMAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

enum {
    PACMAN_STANDALONE_PROGRAM_ROM_SIZE = 0x4000,
    PACMAN_STANDALONE_TILE_ROM_SIZE = 0x1000,
    PACMAN_STANDALONE_SPRITE_ROM_SIZE = 0x1000,
    PACMAN_STANDALONE_PALETTE_PROM_SIZE = 0x0020,
    PACMAN_STANDALONE_COLOR_LOOKUP_PROM_SIZE = 0x0100,
    PACMAN_STANDALONE_SOUND_WAVEFORM_PROM_SIZE = 0x0100,
    PACMAN_STANDALONE_SOUND_TIMING_PROM_SIZE = 0x0100
};

typedef struct {
    const uint8_t *program;
    size_t program_size;

    const uint8_t *tile;
    size_t tile_size;

    const uint8_t *sprite;
    size_t sprite_size;

    const uint8_t *palette;
    size_t palette_size;

    const uint8_t *color_lookup;
    size_t color_lookup_size;

    const uint8_t *sound_waveform;
    size_t sound_waveform_size;

    const uint8_t *sound_timing;
    size_t sound_timing_size;
} PacmanStandaloneRomSet;

bool pacman_standalone_init(
    const PacmanStandaloneRomSet *roms
);

void pacman_standalone_reset(void);

bool pacman_standalone_frame(
    PacmanStandaloneInput input
);

void pacman_standalone_run(void);

#endif
