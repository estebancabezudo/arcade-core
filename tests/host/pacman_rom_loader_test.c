#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pacman_rom_loader.h"

static void write_file(
    const char *path,
    size_t size,
    uint8_t value
) {
    FILE *file =
        fopen(
            path,
            "wb"
        );

    assert(file != NULL);

    for (
        size_t i = 0;
        i < size;
        i++
    ) {
        assert(
            fwrite(
                &value,
                1,
                1,
                file
            ) == 1
        );
    }

    fclose(file);
}

int main(void)
{
    const char *directory =
        "build/host/pacman-rom-test";

    assert(
        system(
            "rm -rf build/host/pacman-rom-test && "
            "mkdir -p build/host/pacman-rom-test"
        ) == 0
    );

    write_file(
        "build/host/pacman-rom-test/pacman.6e",
        PACMAN_PROGRAM_ROM_CHIP_SIZE,
        0x11
    );

    write_file(
        "build/host/pacman-rom-test/pacman.6f",
        PACMAN_PROGRAM_ROM_CHIP_SIZE,
        0x22
    );

    write_file(
        "build/host/pacman-rom-test/pacman.6h",
        PACMAN_PROGRAM_ROM_CHIP_SIZE,
        0x33
    );

    write_file(
        "build/host/pacman-rom-test/pacman.6j",
        PACMAN_PROGRAM_ROM_CHIP_SIZE,
        0x44
    );

    write_file(
        "build/host/pacman-rom-test/pacman.5e",
        PACMAN_ROMSET_TILE_ROM_SIZE,
        0x55
    );

    write_file(
        "build/host/pacman-rom-test/pacman.5f",
        PACMAN_ROMSET_SPRITE_ROM_SIZE,
        0x66
    );

    write_file(
        "build/host/pacman-rom-test/82s123.7f",
        PACMAN_ROMSET_PALETTE_PROM_SIZE,
        0x77
    );

    write_file(
        "build/host/pacman-rom-test/82s126.4a",
        PACMAN_ROMSET_COLOR_LOOKUP_PROM_SIZE,
        0x88
    );

    write_file(
        "build/host/pacman-rom-test/82s126.1m",
        PACMAN_ROMSET_SOUND_WAVEFORM_PROM_SIZE,
        0x99
    );

    write_file(
        "build/host/pacman-rom-test/82s126.3m",
        PACMAN_ROMSET_SOUND_TIMING_PROM_SIZE,
        0xaa
    );

    PacmanRomSet romset;

    assert(
        host_load_pacman_romset(
            directory,
            &romset
        )
    );

    assert(romset.program[0x0000] == 0x11);
    assert(romset.program[0x1000] == 0x22);
    assert(romset.program[0x2000] == 0x33);
    assert(romset.program[0x3000] == 0x44);

    assert(romset.tile_rom[0] == 0x55);
    assert(romset.sprite_rom[0] == 0x66);

    assert(romset.palette_prom[0] == 0x77);
    assert(romset.color_lookup_prom[0] == 0x88);

    assert(romset.sound_waveform_prom[0] == 0x99);
    assert(
        romset.sound_waveform_prom[
            PACMAN_ROMSET_SOUND_WAVEFORM_PROM_SIZE - 1u
        ] == 0x99
    );

    assert(romset.sound_timing_prom[0] == 0xaa);
    assert(
        romset.sound_timing_prom[
            PACMAN_ROMSET_SOUND_TIMING_PROM_SIZE - 1u
        ] == 0xaa
    );

    assert(
        system(
            "rm -rf build/host/pacman-rom-test"
        ) == 0
    );

    puts(
        "PASS host Pac-Man ten-chip ROM loader"
    );

    return 0;
}
