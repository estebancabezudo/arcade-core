#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pacman_video_board.h"

static void test_initialization(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};
    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    PacmanVideoBoard board;

    assert(
        pacman_video_board_init(
            &board,
            tile_rom,
            sizeof(tile_rom),
            sprite_rom,
            sizeof(sprite_rom),
            palette,
            sizeof(palette),
            lookup,
            sizeof(lookup)
        )
    );

    assert(!board.flip_screen);

    assert(
        !pacman_video_board_init(
            &board,
            tile_rom,
            sizeof(tile_rom) - 1u,
            sprite_rom,
            sizeof(sprite_rom),
            palette,
            sizeof(palette),
            lookup,
            sizeof(lookup)
        )
    );
}

static void test_ram_interface(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};
    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    PacmanVideoBoard board;

    assert(
        pacman_video_board_init(
            &board,
            tile_rom,
            sizeof(tile_rom),
            sprite_rom,
            sizeof(sprite_rom),
            palette,
            sizeof(palette),
            lookup,
            sizeof(lookup)
        )
    );

    pacman_video_board_video_ram_write(
        &board,
        3,
        0x11
    );

    assert(
        pacman_video_board_video_ram_read(
            &board,
            3
        ) == 0x11
    );

    pacman_video_board_color_ram_write(
        &board,
        4,
        0x22
    );

    assert(
        pacman_video_board_color_ram_read(
            &board,
            4
        ) == 0x22
    );

    pacman_video_board_sprite_attribute_write(
        &board,
        5,
        0x33
    );

    assert(
        pacman_video_board_sprite_attribute_read(
            &board,
            5
        ) == 0x33
    );

    pacman_video_board_sprite_coordinate_write(
        &board,
        6,
        0x44
    );

    assert(
        board.sprite_coordinate_ram[6] ==
        0x44
    );

    pacman_video_board_set_flip_screen(
        &board,
        true
    );

    assert(board.flip_screen);

    board.video_ram[0] = 0xa5;

    pacman_video_board_reset(
        &board
    );

    assert(!board.flip_screen);
    assert(board.video_ram[0] == 0xa5);
}

static void test_tile_decode(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};
    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    PacmanVideoBoard board;

    const uint8_t tile = 2;
    const uint16_t base =
        (uint16_t)tile * 16u;

    /*
     * Graphics offsets are MSB-first.
     *
     * y=1, x=0 -> pixel 1:
     * plane 0 = physical bit 7.
     *
     * y=1, x=1 -> pixel 2:
     * plane 1 = physical bit 2.
     */
    tile_rom[base + 8u + 1u] =
        0x48u;

    /*
     * y=1, x=5 -> pixel 3.
     *
     * Logical bit 1:
     * plane 0 = physical bit 6
     * plane 1 = physical bit 2
     */
    tile_rom[base + 1u] =
        0x44u;

    assert(
        pacman_video_board_init(
            &board,
            tile_rom,
            sizeof(tile_rom),
            sprite_rom,
            sizeof(sprite_rom),
            palette,
            sizeof(palette),
            lookup,
            sizeof(lookup)
        )
    );

    assert(
        pacman_video_board_tile_pixel(
            &board,
            tile,
            0,
            1
        ) == 1
    );

    assert(
        pacman_video_board_tile_pixel(
            &board,
            tile,
            1,
            1
        ) == 2
    );

    assert(
        pacman_video_board_tile_pixel(
            &board,
            tile,
            5,
            1
        ) == 3
    );

    assert(
        pacman_video_board_tile_pixel(
            &board,
            tile,
            2,
            1
        ) == 0
    );
}

static void test_sprite_decode(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};
    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    PacmanVideoBoard board;

    const uint8_t sprite = 3;
    const uint16_t base =
        (uint16_t)sprite * 64u;

    /*
     * x=0, y=0 -> pixel 1.
     *
     * Logical bit 0, plane 0 -> physical bit 7.
     */
    sprite_rom[base + 8u] =
        0x08u;

    /*
     * x=5, y=0 -> pixel 2.
     *
     * Logical bit 1, plane 1 -> physical bit 2.
     */
    sprite_rom[base + 16u] =
        0x40u;

    /*
     * x=10, y=9 -> pixel 3.
     *
     * Logical bit 2:
     * plane 0 -> physical bit 5
     * plane 1 -> physical bit 1
     */
    sprite_rom[
        base +
        24u +
        33u
    ] =
        0x22u;

    assert(
        pacman_video_board_init(
            &board,
            tile_rom,
            sizeof(tile_rom),
            sprite_rom,
            sizeof(sprite_rom),
            palette,
            sizeof(palette),
            lookup,
            sizeof(lookup)
        )
    );

    assert(
        pacman_video_board_sprite_pixel(
            &board,
            sprite,
            0,
            0
        ) == 1
    );

    assert(
        pacman_video_board_sprite_pixel(
            &board,
            sprite,
            5,
            0
        ) == 2
    );

    assert(
        pacman_video_board_sprite_pixel(
            &board,
            sprite,
            10,
            9
        ) == 3
    );
}

static void test_palette(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};
    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    PacmanVideoBoard board;

    palette[1] = 0x07;
    palette[2] = 0x38;
    palette[3] = 0xc0;

    lookup[5u * 4u + 0u] = 0;
    lookup[5u * 4u + 1u] = 1;
    lookup[5u * 4u + 2u] = 2;
    lookup[5u * 4u + 3u] = 3;

    assert(
        pacman_video_board_init(
            &board,
            tile_rom,
            sizeof(tile_rom),
            sprite_rom,
            sizeof(sprite_rom),
            palette,
            sizeof(palette),
            lookup,
            sizeof(lookup)
        )
    );

    PacmanRgb red =
        pacman_video_board_pixel_color(
            &board,
            5,
            1
        );

    assert(red.r == 0xff);
    assert(red.g == 0x00);
    assert(red.b == 0x00);

    PacmanRgb green =
        pacman_video_board_pixel_color(
            &board,
            5,
            2
        );

    assert(green.r == 0x00);
    assert(green.g == 0xff);
    assert(green.b == 0x00);

    PacmanRgb blue =
        pacman_video_board_pixel_color(
            &board,
            5,
            3
        );

    assert(blue.r == 0x00);
    assert(blue.g == 0x00);
    assert(blue.b == 0xff);
}

int main(void)
{
    test_initialization();
    test_ram_interface();

    test_tile_decode();
    test_sprite_decode();

    test_palette();

    puts(
        "PASS Pac-Man video RAM, graphics ROM and palette decode"
    );

    return 0;
}
