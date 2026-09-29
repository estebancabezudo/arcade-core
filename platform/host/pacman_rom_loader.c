#include "pacman_rom_loader.h"

#include <stdio.h>

#include "file_loader.h"

enum {
    PATH_BUFFER_SIZE = 4096
};

static bool build_path(
    char *destination,
    size_t destination_size,
    const char *directory,
    const char *filename
) {
    const int written =
        snprintf(
            destination,
            destination_size,
            "%s/%s",
            directory,
            filename
        );

    return
        written >= 0 &&
        (size_t)written < destination_size;
}

static bool load_named_file(
    const char *directory,
    const char *filename,
    uint8_t *destination,
    size_t expected_size
) {
    char path[PATH_BUFFER_SIZE];

    if (
        !build_path(
            path,
            sizeof(path),
            directory,
            filename
        )
    ) {
        return false;
    }

    return host_load_exact_file(
        path,
        destination,
        expected_size
    );
}

bool host_load_pacman_romset(
    const char *directory,
    PacmanRomSet *romset
) {
    if (
        directory == NULL ||
        romset == NULL
    ) {
        return false;
    }

    uint8_t chip0[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip1[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip2[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip3[PACMAN_PROGRAM_ROM_CHIP_SIZE];

    uint8_t tile_rom[PACMAN_ROMSET_TILE_ROM_SIZE];
    uint8_t sprite_rom[PACMAN_ROMSET_SPRITE_ROM_SIZE];

    uint8_t palette_prom[
        PACMAN_ROMSET_PALETTE_PROM_SIZE
    ];

    uint8_t color_lookup_prom[
        PACMAN_ROMSET_COLOR_LOOKUP_PROM_SIZE
    ];

    uint8_t sound_waveform_prom[
        PACMAN_ROMSET_SOUND_WAVEFORM_PROM_SIZE
    ];

    uint8_t sound_timing_prom[
        PACMAN_ROMSET_SOUND_TIMING_PROM_SIZE
    ];

    if (
        !load_named_file(
            directory,
            "pacman.6e",
            chip0,
            sizeof(chip0)
        ) ||
        !load_named_file(
            directory,
            "pacman.6f",
            chip1,
            sizeof(chip1)
        ) ||
        !load_named_file(
            directory,
            "pacman.6h",
            chip2,
            sizeof(chip2)
        ) ||
        !load_named_file(
            directory,
            "pacman.6j",
            chip3,
            sizeof(chip3)
        ) ||
        !load_named_file(
            directory,
            "pacman.5e",
            tile_rom,
            sizeof(tile_rom)
        ) ||
        !load_named_file(
            directory,
            "pacman.5f",
            sprite_rom,
            sizeof(sprite_rom)
        ) ||
        !load_named_file(
            directory,
            "82s123.7f",
            palette_prom,
            sizeof(palette_prom)
        ) ||
        !load_named_file(
            directory,
            "82s126.4a",
            color_lookup_prom,
            sizeof(color_lookup_prom)
        ) ||
        !load_named_file(
            directory,
            "82s126.1m",
            sound_waveform_prom,
            sizeof(sound_waveform_prom)
        ) ||
        !load_named_file(
            directory,
            "82s126.3m",
            sound_timing_prom,
            sizeof(sound_timing_prom)
        )
    ) {
        return false;
    }

    if (
        !pacman_romset_build_program(
            romset,

            chip0,
            sizeof(chip0),

            chip1,
            sizeof(chip1),

            chip2,
            sizeof(chip2),

            chip3,
            sizeof(chip3)
        )
    ) {
        return false;
    }

    if (
        !pacman_romset_set_sound(
            romset,

            sound_waveform_prom,
            sizeof(sound_waveform_prom),

            sound_timing_prom,
            sizeof(sound_timing_prom)
        )
    ) {
        return false;
    }

    return pacman_romset_set_video(
        romset,

        tile_rom,
        sizeof(tile_rom),

        sprite_rom,
        sizeof(sprite_rom),

        palette_prom,
        sizeof(palette_prom),

        color_lookup_prom,
        sizeof(color_lookup_prom)
    );
}
