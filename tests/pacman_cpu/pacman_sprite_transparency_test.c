#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pacman_video_board.h"

int main(void)
{
    uint8_t tile_rom[PACMAN_TILE_ROM_SIZE] = {0};
    uint8_t sprite_rom[PACMAN_SPRITE_ROM_SIZE] = {0};

    uint8_t palette[PACMAN_PALETTE_PROM_SIZE] = {0};
    uint8_t lookup[PACMAN_COLOR_LOOKUP_PROM_SIZE] = {0};

    /*
     * Palette entry 1 = red.
     */
    palette[1] = 0x07;

    /*
     * Background color 3, graphics pen 1 -> palette entry 1.
     */
    lookup[
        3u * 4u +
        1u
    ] = 1;

    /*
     * Sprite color 0, graphics pen 1 -> palette entry 0.
     *
     * The lookup array is zero-initialized, so this is
     * intentionally transparent.
     */

    /*
     * Tile 1, local pixel 0,0 = graphics pen 1.
     *
     * Pac-Man graphics are MSB-first and plane 1 is the
     * low significance plane, therefore pen 1 uses bit 3.
     */
    tile_rom[
        1u * 16u +
        8u
    ] = 0x08;

    /*
     * Sprite 1, local pixel 0,0 = graphics pen 1.
     */
    sprite_rom[
        1u * 64u +
        8u
    ] = 0x08;

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

    /*
     * Put the red background tile underneath the sprite.
     */
    const uint16_t tile_offset =
        pacman_video_board_tile_offset(
            12,
            6
        );

    board.video_ram[tile_offset] = 1;
    board.color_ram[tile_offset] = 3;

    /*
     * Sprite entry at offset 6:
     *
     * sprite number 1
     * color 0
     * screen position 96,48
     */
    board.sprite_attribute_ram[6] =
        (uint8_t)(1u << 2);

    board.sprite_attribute_ram[7] = 0;

    board.sprite_coordinate_ram[6] =
        (uint8_t)(48 + 31);

    board.sprite_coordinate_ram[7] =
        (uint8_t)(272 - 96);

    PacmanRgb sprite_color;

    /*
     * Although the graphics ROM pixel is pen 1, its lookup PROM
     * result is palette entry 0, so no opaque sprite exists here.
     */
    assert(
        !pacman_video_board_sprite_layer_pixel(
            &board,
            96,
            48,
            &sprite_color
        )
    );

    /*
     * The red background must therefore remain visible.
     */
    const PacmanRgb output =
        pacman_video_board_output_pixel(
            &board,
            96,
            48
        );

    assert(output.r == 0xff);
    assert(output.g == 0x00);
    assert(output.b == 0x00);

    puts(
        "PASS Pac-Man sprite transparency after color lookup"
    );

    return 0;
}
