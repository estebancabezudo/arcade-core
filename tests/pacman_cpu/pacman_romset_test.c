#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pacman_romset.h"

static void fill_chip(
    uint8_t *chip,
    uint8_t marker
) {
    memset(
        chip,
        marker,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );
}

static void test_program_layout(void)
{
    uint8_t chip0[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip1[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip2[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip3[PACMAN_PROGRAM_ROM_CHIP_SIZE];

    fill_chip(chip0, 0x11);
    fill_chip(chip1, 0x22);
    fill_chip(chip2, 0x33);
    fill_chip(chip3, 0x44);

    PacmanRomSet romset;

    assert(
        pacman_romset_build_program(
            &romset,

            chip0,
            sizeof(chip0),

            chip1,
            sizeof(chip1),

            chip2,
            sizeof(chip2),

            chip3,
            sizeof(chip3)
        )
    );

    assert(romset.program[0x0000] == 0x11);
    assert(romset.program[0x0fff] == 0x11);

    assert(romset.program[0x1000] == 0x22);
    assert(romset.program[0x1fff] == 0x22);

    assert(romset.program[0x2000] == 0x33);
    assert(romset.program[0x2fff] == 0x33);

    assert(romset.program[0x3000] == 0x44);
    assert(romset.program[0x3fff] == 0x44);
}

static void test_rejects_invalid_chips(void)
{
    uint8_t chip[PACMAN_PROGRAM_ROM_CHIP_SIZE] = {0};

    PacmanRomSet romset;

    assert(
        !pacman_romset_build_program(
            &romset,

            chip,
            sizeof(chip),

            chip,
            sizeof(chip) - 1u,

            chip,
            sizeof(chip),

            chip,
            sizeof(chip)
        )
    );

    assert(
        !pacman_romset_build_program(
            NULL,

            chip,
            sizeof(chip),

            chip,
            sizeof(chip),

            chip,
            sizeof(chip),

            chip,
            sizeof(chip)
        )
    );
}

int main(void)
{
    test_program_layout();
    test_rejects_invalid_chips();

    puts("PASS Pac-Man program ROM set");

    return 0;
}
