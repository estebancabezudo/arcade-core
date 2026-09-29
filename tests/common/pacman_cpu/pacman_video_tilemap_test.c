#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "pacman_video_board.h"

static PacmanVideoBoard create_board(
    uint8_t *tile_rom,
    uint8_t *sprite_rom,
    uint8_t *palette,
    uint8_t *lookup
) {
    PacmanVideoBoard board;

    assert(
        pacman_video_board_init(
            &board,

            tile_rom,
            PACMAN_TILE_ROM_SIZE,

            sprite_rom,
            PACMAN_SPRITE_ROM_SIZE,

            palette,
            PACMAN_PALETTE_PROM_SIZE,

            lookup,
            PACMAN_COLOR_LOOKUP_PROM_SIZE
        )
    );

    return board;
}

static void test_tile_mapper(void)
{
    /*
     * Main 32-column body starts at physical row 2.
     */
    assert(
        pacman_video_board_tile_offset(
            2,
            0
        ) == 64
    );

    assert(
        pacman_video_board_tile_offset(
            33,
            0
        ) == 95
    );

    /*
     * Left two visible columns live at the high end
     * of the physical 32x32 RAM.
     */
    assert(
        pacman_video_board_tile_offset(
            0,
            0
        ) == 962
    );

    assert(
        pacman_video_board_tile_offset(
            1,
            0
        ) == 994
    );

    /*
     * Right two visible columns live at the beginning.
     */
    assert(
        pacman_video_board_tile_offset(
            34,
            0
        ) == 2
    );

    assert(
        pacman_video_board_tile_offset(
            35,
            0
        ) == 34
    );

    /*
     * Bottom-right visible tile.
     */
    assert(
        pacman_video_board_tile_offset(
            35,
            27
        ) == 61
    );

    assert(
        pacman_video_board_tile_offset(
            36,
            0
        ) == 0xffff
    );
}

static void test_background_pixel(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};

    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    /*
     * Palette:
     *
     * 1 = red
     * 2 = green
     */
    palette[1] = 0x07;
    palette[2] = 0x38;

    /*
     * Color code 3:
     *
     * pixel 1 -> red
     * pixel 2 -> green
     */
    lookup[3u * 4u + 1u] = 1;
    lookup[3u * 4u + 2u] = 2;

    /*
     * Tile 5.
     *
     * At x=0,y=0 produce pixel value 1.
     */
    tile_rom[
        5u * 16u +
        8u
    ] =
        0x08;

    /*
     * At x=7,y=7 produce pixel value 2.
     *
     * x 7 lives in the first byte group,
     * bit 3, plane 1 -> bit 7.
     */
    tile_rom[
        5u * 16u +
        7u
    ] =
        0x10;

    PacmanVideoBoard board =
        create_board(
            tile_rom,
            sprite_rom,
            palette,
            lookup
        );

    /*
     * Logical tile 2,0 maps to RAM offset 64.
     */
    board.video_ram[64] = 5;
    board.color_ram[64] = 3;

    PacmanRgb pixel =
        pacman_video_board_background_pixel(
            &board,
            16,
            0
        );

    assert(pixel.r == 0xff);
    assert(pixel.g == 0x00);
    assert(pixel.b == 0x00);

    pixel =
        pacman_video_board_background_pixel(
            &board,
            23,
            7
        );

    assert(pixel.r == 0x00);
    assert(pixel.g == 0xff);
    assert(pixel.b == 0x00);
}

static void test_flip_screen(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};

    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    palette[1] = 0x07;
    lookup[4u * 4u + 1u] = 1;

    /*
     * Tile 7, pixel 7,7 = color pixel 1.
     */
    tile_rom[
        7u * 16u +
        7u
    ] =
        0x01;

    PacmanVideoBoard board =
        create_board(
            tile_rom,
            sprite_rom,
            palette,
            lookup
        );

    /*
     * With flip enabled, screen coordinate 0,0 samples
     * physical coordinate 287,223:
     *
     * tile 35,27
     * local pixel 7,7
     */
    const uint16_t offset =
        pacman_video_board_tile_offset(
            35,
            27
        );

    board.video_ram[offset] = 7;
    board.color_ram[offset] = 4;

    pacman_video_board_set_flip_screen(
        &board,
        true
    );

    const PacmanRgb pixel =
        pacman_video_board_background_pixel(
            &board,
            0,
            0
        );

    assert(pixel.r == 0xff);
    assert(pixel.g == 0);
    assert(pixel.b == 0);
}

int main(void)
{
    test_tile_mapper();
    test_background_pixel();
    test_flip_screen();

    puts(
        "PASS Pac-Man tilemap mapping and background raster"
    );

    return 0;
}
