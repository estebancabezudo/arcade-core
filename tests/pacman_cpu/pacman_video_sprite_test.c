#include <assert.h>
#include <stdbool.h>
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

static void define_sprite_pixel(
    uint8_t *sprite_rom,
    uint8_t sprite,
    uint8_t pixel
) {
    /*
     * Local sprite pixel 0,0.
     */
    uint8_t packed = 0u;

    if ((pixel & 1u) != 0u) {
        packed |= 0x08u;
    }

    if ((pixel & 2u) != 0u) {
        packed |= 0x80u;
    }

    sprite_rom[
        (uint16_t)sprite * 64u +
        8u
    ] = packed;
}

static void place_sprite(
    PacmanVideoBoard *board,
    uint8_t offset,
    uint8_t sprite,
    uint8_t color,
    int x,
    int y
) {
    board->sprite_attribute_ram[offset] =
        (uint8_t)(
            sprite << 2
        );

    board->sprite_attribute_ram[
        offset + 1u
    ] =
        color;

    board->sprite_coordinate_ram[offset] =
        (uint8_t)(
            y + 31
        );

    board->sprite_coordinate_ram[
        offset + 1u
    ] =
        (uint8_t)(
            272 - x
        );
}

static void test_sprite_over_background(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};

    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    /*
     * Background palette entry 1 = red.
     */
    palette[1] = 0x07;

    /*
     * Sprite palette entry 2 = green.
     */
    palette[2] = 0x38;

    /*
     * Background color 3 / pixel 1 -> palette 1.
     */
    lookup[3u * 4u + 1u] = 1;

    /*
     * Sprite color 5 / pixel 1 -> palette entry 2.
     */
    lookup[5u * 4u + 1u] = 2;

    /*
     * Tile 2: local pixel 0,0 = pen 1.
     */
    /*
     * Local pixels 0,0 and 1,0 are both background pen 1.
     * This lets us verify that sprite transparency exposes
     * the underlying red background at x+1.
     */
    tile_rom[
        2u * 16u +
        8u
    ] = 0x0c;

    /*
     * Sprite 1: local pixel 0,0 = pen 1.
     */
    define_sprite_pixel(
        sprite_rom,
        1,
        1
    );

    PacmanVideoBoard board =
        create_board(
            tile_rom,
            sprite_rom,
            palette,
            lookup
        );

    /*
     * Background tile covering x=96,y=48.
     */
    const uint8_t tile_x =
        96u / 8u;

    const uint8_t tile_y =
        48u / 8u;

    const uint16_t tile_offset =
        pacman_video_board_tile_offset(
            tile_x,
            tile_y
        );

    board.video_ram[tile_offset] = 2;
    board.color_ram[tile_offset] = 3;

    /*
     * Use offset 6 to avoid the low-entry positioning quirk.
     */
    place_sprite(
        &board,
        6,
        1,
        5,
        96,
        48
    );

    PacmanRgb background =
        pacman_video_board_background_pixel(
            &board,
            96,
            48
        );

    assert(background.r == 0xff);
    assert(background.g == 0x00);
    assert(background.b == 0x00);

    PacmanRgb output =
        pacman_video_board_output_pixel(
            &board,
            96,
            48
        );

    assert(output.r == 0x00);
    assert(output.g == 0xff);
    assert(output.b == 0x00);

    /*
     * Pen zero elsewhere in the sprite is transparent,
     * so background remains visible.
     */
    output =
        pacman_video_board_output_pixel(
            &board,
            97,
            48
        );

    assert(output.r == 0xff);
    assert(output.g == 0x00);
    assert(output.b == 0x00);
}

static void test_sprite_flip(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};

    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    palette[1] = 0x38;
    lookup[5u * 4u + 1u] = 1;

    define_sprite_pixel(
        sprite_rom,
        1,
        1
    );

    PacmanVideoBoard board =
        create_board(
            tile_rom,
            sprite_rom,
            palette,
            lookup
        );

    place_sprite(
        &board,
        6,
        1,
        5,
        100,
        60
    );

    /*
     * Enable horizontal flip.
     */
    board.sprite_attribute_ram[6] |= 0x01;

    PacmanRgb pixel;

    assert(
        !pacman_video_board_sprite_layer_pixel(
            &board,
            100,
            60,
            &pixel
        )
    );

    assert(
        pacman_video_board_sprite_layer_pixel(
            &board,
            115,
            60,
            &pixel
        )
    );

    assert(pixel.g == 0xff);
}

static void test_sprite_priority(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};

    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    /*
     * Sprite palette:
     * entry 1 = green
     * entry 2 = blue
     */
    palette[1] = 0x38;
    palette[2] = 0xc0;

    lookup[5u * 4u + 1u] = 1;
    lookup[6u * 4u + 1u] = 2;

    define_sprite_pixel(
        sprite_rom,
        1,
        1
    );

    define_sprite_pixel(
        sprite_rom,
        2,
        1
    );

    PacmanVideoBoard board =
        create_board(
            tile_rom,
            sprite_rom,
            palette,
            lookup
        );

    /*
     * Offset 14 is drawn earlier.
     */
    place_sprite(
        &board,
        14,
        1,
        5,
        120,
        80
    );

    /*
     * Offset 6 is drawn later and therefore wins.
     */
    place_sprite(
        &board,
        6,
        2,
        6,
        120,
        80
    );

    PacmanRgb pixel;

    assert(
        pacman_video_board_sprite_layer_pixel(
            &board,
            120,
            80,
            &pixel
        )
    );

    assert(pixel.r == 0x00);
    assert(pixel.g == 0x00);
    assert(pixel.b == 0xff);
}

static void test_sprite_clip(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};

    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    palette[1] = 0x38;
    lookup[5u * 4u + 1u] = 1;

    define_sprite_pixel(
        sprite_rom,
        1,
        1
    );

    PacmanVideoBoard board =
        create_board(
            tile_rom,
            sprite_rom,
            palette,
            lookup
        );

    place_sprite(
        &board,
        6,
        1,
        5,
        10,
        40
    );

    PacmanRgb pixel;

    /*
     * Sprite hardware is clipped outside x=16..271.
     */
    assert(
        !pacman_video_board_sprite_layer_pixel(
            &board,
            10,
            40,
            &pixel
        )
    );
}

int main(void)
{
    test_sprite_over_background();
    test_sprite_flip();
    test_sprite_priority();
    test_sprite_clip();

    puts(
        "PASS Pac-Man sprite layer and video mixer"
    );

    return 0;
}
