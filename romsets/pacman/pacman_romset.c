#include "pacman_romset.h"

#include <string.h>

static bool valid_data(
    const uint8_t *data,
    size_t actual_size,
    size_t expected_size
) {
    return
        data != NULL &&
        actual_size == expected_size;
}

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
) {
    if (
        romset == NULL ||
        !valid_data(
            chip0,
            chip0_size,
            PACMAN_PROGRAM_ROM_CHIP_SIZE
        ) ||
        !valid_data(
            chip1,
            chip1_size,
            PACMAN_PROGRAM_ROM_CHIP_SIZE
        ) ||
        !valid_data(
            chip2,
            chip2_size,
            PACMAN_PROGRAM_ROM_CHIP_SIZE
        ) ||
        !valid_data(
            chip3,
            chip3_size,
            PACMAN_PROGRAM_ROM_CHIP_SIZE
        )
    ) {
        return false;
    }

    memcpy(
        &romset->program[0x0000],
        chip0,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );

    memcpy(
        &romset->program[0x1000],
        chip1,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );

    memcpy(
        &romset->program[0x2000],
        chip2,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );

    memcpy(
        &romset->program[0x3000],
        chip3,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );

    return true;
}

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
) {
    if (
        romset == NULL ||
        !valid_data(
            tile_rom,
            tile_rom_size,
            PACMAN_ROMSET_TILE_ROM_SIZE
        ) ||
        !valid_data(
            sprite_rom,
            sprite_rom_size,
            PACMAN_ROMSET_SPRITE_ROM_SIZE
        ) ||
        !valid_data(
            palette_prom,
            palette_prom_size,
            PACMAN_ROMSET_PALETTE_PROM_SIZE
        ) ||
        !valid_data(
            color_lookup_prom,
            color_lookup_prom_size,
            PACMAN_ROMSET_COLOR_LOOKUP_PROM_SIZE
        )
    ) {
        return false;
    }

    memcpy(
        romset->tile_rom,
        tile_rom,
        PACMAN_ROMSET_TILE_ROM_SIZE
    );

    memcpy(
        romset->sprite_rom,
        sprite_rom,
        PACMAN_ROMSET_SPRITE_ROM_SIZE
    );

    memcpy(
        romset->palette_prom,
        palette_prom,
        PACMAN_ROMSET_PALETTE_PROM_SIZE
    );

    memcpy(
        romset->color_lookup_prom,
        color_lookup_prom,
        PACMAN_ROMSET_COLOR_LOOKUP_PROM_SIZE
    );

    return true;
}

bool pacman_romset_set_sound(
    PacmanRomSet *romset,

    const uint8_t *waveform_prom,
    size_t waveform_prom_size,

    const uint8_t *timing_prom,
    size_t timing_prom_size
) {
    if (
        romset == NULL ||
        !valid_data(
            waveform_prom,
            waveform_prom_size,
            PACMAN_ROMSET_SOUND_WAVEFORM_PROM_SIZE
        ) ||
        !valid_data(
            timing_prom,
            timing_prom_size,
            PACMAN_ROMSET_SOUND_TIMING_PROM_SIZE
        )
    ) {
        return false;
    }

    memcpy(
        romset->sound_waveform_prom,
        waveform_prom,
        PACMAN_ROMSET_SOUND_WAVEFORM_PROM_SIZE
    );

    memcpy(
        romset->sound_timing_prom,
        timing_prom,
        PACMAN_ROMSET_SOUND_TIMING_PROM_SIZE
    );

    return true;
}
