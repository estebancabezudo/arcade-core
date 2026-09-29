#include "pacman_video_board.h"

#include <string.h>

static bool valid_rom(
    const uint8_t *data,
    size_t actual_size,
    size_t expected_size
) {
    return
        data != NULL &&
        actual_size == expected_size;
}

bool pacman_video_board_init(
    PacmanVideoBoard *board,

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
        board == NULL ||
        !valid_rom(
            tile_rom,
            tile_rom_size,
            PACMAN_TILE_ROM_SIZE
        ) ||
        !valid_rom(
            sprite_rom,
            sprite_rom_size,
            PACMAN_SPRITE_ROM_SIZE
        ) ||
        !valid_rom(
            palette_prom,
            palette_prom_size,
            PACMAN_PALETTE_PROM_SIZE
        ) ||
        !valid_rom(
            color_lookup_prom,
            color_lookup_prom_size,
            PACMAN_COLOR_LOOKUP_PROM_SIZE
        )
    ) {
        return false;
    }

    memset(
        board,
        0,
        sizeof(*board)
    );

    board->tile_rom = tile_rom;
    board->sprite_rom = sprite_rom;

    board->palette_prom = palette_prom;
    board->color_lookup_prom =
        color_lookup_prom;

    return true;
}

void pacman_video_board_reset(
    PacmanVideoBoard *board
) {
    if (board == NULL) {
        return;
    }

    /*
     * External RAM is not cleared by reset.
     */
    board->flip_screen = false;
}

uint8_t pacman_video_board_video_ram_read(
    void *context,
    uint16_t offset
) {
    PacmanVideoBoard *board = context;

    if (
        board == NULL ||
        offset >=
            PACMAN_VIDEO_BOARD_VIDEO_RAM_SIZE
    ) {
        return 0xff;
    }

    return board->video_ram[offset];
}

void pacman_video_board_video_ram_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    PacmanVideoBoard *board = context;

    if (
        board == NULL ||
        offset >=
            PACMAN_VIDEO_BOARD_VIDEO_RAM_SIZE
    ) {
        return;
    }

    board->video_ram[offset] = value;
}

uint8_t pacman_video_board_color_ram_read(
    void *context,
    uint16_t offset
) {
    PacmanVideoBoard *board = context;

    if (
        board == NULL ||
        offset >=
            PACMAN_VIDEO_BOARD_COLOR_RAM_SIZE
    ) {
        return 0xff;
    }

    return board->color_ram[offset];
}

void pacman_video_board_color_ram_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    PacmanVideoBoard *board = context;

    if (
        board == NULL ||
        offset >=
            PACMAN_VIDEO_BOARD_COLOR_RAM_SIZE
    ) {
        return;
    }

    board->color_ram[offset] = value;
}

uint8_t pacman_video_board_sprite_attribute_read(
    void *context,
    uint16_t offset
) {
    PacmanVideoBoard *board = context;

    if (
        board == NULL ||
        offset >=
            PACMAN_VIDEO_BOARD_SPRITE_ATTRIBUTE_RAM_SIZE
    ) {
        return 0xff;
    }

    return
        board->sprite_attribute_ram[offset];
}

void pacman_video_board_sprite_attribute_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    PacmanVideoBoard *board = context;

    if (
        board == NULL ||
        offset >=
            PACMAN_VIDEO_BOARD_SPRITE_ATTRIBUTE_RAM_SIZE
    ) {
        return;
    }

    board->sprite_attribute_ram[offset] =
        value;
}

void pacman_video_board_sprite_coordinate_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    PacmanVideoBoard *board = context;

    if (
        board == NULL ||
        offset >=
            PACMAN_VIDEO_BOARD_SPRITE_COORDINATE_RAM_SIZE
    ) {
        return;
    }

    board->sprite_coordinate_ram[offset] =
        value;
}

void pacman_video_board_set_flip_screen(
    void *context,
    bool enabled
) {
    PacmanVideoBoard *board = context;

    if (board == NULL) {
        return;
    }

    board->flip_screen = enabled;
}

uint8_t pacman_video_board_tile_pixel(
    const PacmanVideoBoard *board,
    uint8_t tile,
    uint8_t x,
    uint8_t y
) {
    if (
        board == NULL ||
        x >= PACMAN_TILE_WIDTH ||
        y >= PACMAN_TILE_HEIGHT
    ) {
        return 0u;
    }

    /*
     * MAME layout:
     *
     * x offsets:
     *   64,65,66,67, 0,1,2,3
     *
     * y offsets:
     *   0,8,16,...56
     *
     * planes:
     *   0,4
     *
     * 16 bytes per tile.
     */
    const uint16_t base =
        (uint16_t)tile * 16u;

    uint16_t byte_offset;
    uint8_t bit;

    if (x < 4u) {
        byte_offset =
            (uint16_t)(
                base +
                8u +
                y
            );

        bit = x;
    } else {
        byte_offset =
            (uint16_t)(
                base +
                y
            );

        bit =
            (uint8_t)(x - 4u);
    }

    const uint8_t packed =
        board->tile_rom[byte_offset];

    /*
     * MAME graphics bit offsets are numbered from the
     * most-significant bit of each byte.
     *
     * For logical bit offsets 0..3:
     *
     * plane 0 -> physical bits 7..4
     * plane 1 -> physical bits 3..0
     */
    const uint8_t plane0 =
        (uint8_t)(
            (packed >> (7u - bit)) &
            1u
        );

    const uint8_t plane1 =
        (uint8_t)(
            (packed >> (3u - bit)) &
            1u
        );

    return
        (uint8_t)(
            (plane0 << 1) |
            plane1
        );
}

uint8_t pacman_video_board_sprite_pixel(
    const PacmanVideoBoard *board,
    uint8_t sprite,
    uint8_t x,
    uint8_t y
) {
    if (
        board == NULL ||
        x >= PACMAN_SPRITE_WIDTH ||
        y >= PACMAN_SPRITE_HEIGHT
    ) {
        return 0u;
    }

    /*
     * Pac-Man has 64 sprites in the 4 KiB sprite ROM.
     */
    const uint16_t base =
        (uint16_t)(sprite & 0x3fu) *
        64u;

    uint16_t x_byte;

    if (x < 4u) {
        x_byte = 8u;
    } else if (x < 8u) {
        x_byte = 16u;
    } else if (x < 12u) {
        x_byte = 24u;
    } else {
        x_byte = 0u;
    }

    const uint16_t y_byte =
        y < 8u
            ? y
            : (uint16_t)(
                32u +
                (y - 8u)
            );

    const uint8_t bit =
        (uint8_t)(x & 3u);

    const uint8_t packed =
        board->sprite_rom[
            base +
            x_byte +
            y_byte
        ];

    /*
     * MAME graphics bit offsets are numbered from the
     * most-significant bit of each byte.
     *
     * For logical bit offsets 0..3:
     *
     * plane 0 -> physical bits 7..4
     * plane 1 -> physical bits 3..0
     */
    const uint8_t plane0 =
        (uint8_t)(
            (packed >> (7u - bit)) &
            1u
        );

    const uint8_t plane1 =
        (uint8_t)(
            (packed >> (3u - bit)) &
            1u
        );

    return
        (uint8_t)(
            (plane0 << 1) |
            plane1
        );
}

uint8_t pacman_video_board_palette_index(
    const PacmanVideoBoard *board,
    uint8_t color,
    uint8_t pixel
) {
    if (board == NULL) {
        return 0u;
    }

    /*
     * Pac-Man uses a 5-bit color attribute.
     * Four lookup entries exist per color.
     */
    const uint16_t lookup =
        (uint16_t)(
            ((color & 0x1fu) << 2) |
            (pixel & 0x03u)
        );

    return
        board->color_lookup_prom[lookup] &
        0x0fu;
}

PacmanRgb pacman_video_board_palette_color(
    const PacmanVideoBoard *board,
    uint8_t palette_index
) {
    PacmanRgb color = {0, 0, 0};

    if (board == NULL) {
        return color;
    }

    const uint8_t value =
        board->palette_prom[
            palette_index & 0x1fu
        ];

    /*
     * Resistor-network weights normalized to 0..255:
     *
     * R/G: 1k, 470, 220 ohm
     *      -> 0x21, 0x47, 0x97
     *
     * B:   470, 220 ohm
     *      -> 0x51, 0xAE
     */
    color.r =
        (uint8_t)(
            ((value >> 0) & 1u) * 0x21u +
            ((value >> 1) & 1u) * 0x47u +
            ((value >> 2) & 1u) * 0x97u
        );

    color.g =
        (uint8_t)(
            ((value >> 3) & 1u) * 0x21u +
            ((value >> 4) & 1u) * 0x47u +
            ((value >> 5) & 1u) * 0x97u
        );

    color.b =
        (uint8_t)(
            ((value >> 6) & 1u) * 0x51u +
            ((value >> 7) & 1u) * 0xaeu
        );

    return color;
}

PacmanRgb pacman_video_board_pixel_color(
    const PacmanVideoBoard *board,
    uint8_t color,
    uint8_t pixel
) {
    return pacman_video_board_palette_color(
        board,
        pacman_video_board_palette_index(
            board,
            color,
            pixel
        )
    );
}

uint16_t pacman_video_board_tile_offset(
    uint8_t tile_x,
    uint8_t tile_y
) {
    if (
        tile_x >= 36u ||
        tile_y >= 28u
    ) {
        return 0xffffu;
    }

    /*
     * Original Pac-Man tile address mapper.
     *
     * Logical tilemap:
     *
     *   36 columns x 28 rows
     *
     * Physical RAM:
     *
     *   32 x 32 = 1024 entries
     *
     * The extra logical columns are mapped into the otherwise
     * unused top/bottom rows of the 32x32 physical RAM layout.
     */
    const int row =
        (int)tile_y + 2;

    const int col =
        (int)tile_x - 2;

    int offset;

    if ((col & 0x20) != 0) {
        offset =
            row +
            ((col & 0x1f) << 5);
    } else {
        offset =
            col +
            (row << 5);
    }

    return (uint16_t)offset;
}

PacmanRgb pacman_video_board_background_pixel(
    const PacmanVideoBoard *board,
    uint16_t x,
    uint16_t y
) {
    const PacmanRgb black = {
        .r = 0,
        .g = 0,
        .b = 0
    };

    if (
        board == NULL ||
        x >= PACMAN_NATIVE_WIDTH ||
        y >= PACMAN_NATIVE_HEIGHT
    ) {
        return black;
    }

    /*
     * The hardware flip control reverses both axes.
     */
    if (board->flip_screen) {
        x =
            (uint16_t)(
                PACMAN_NATIVE_WIDTH -
                1u -
                x
            );

        y =
            (uint16_t)(
                PACMAN_NATIVE_HEIGHT -
                1u -
                y
            );
    }

    const uint8_t tile_x =
        (uint8_t)(
            x /
            PACMAN_TILE_WIDTH
        );

    const uint8_t tile_y =
        (uint8_t)(
            y /
            PACMAN_TILE_HEIGHT
        );

    const uint8_t pixel_x =
        (uint8_t)(
            x %
            PACMAN_TILE_WIDTH
        );

    const uint8_t pixel_y =
        (uint8_t)(
            y %
            PACMAN_TILE_HEIGHT
        );

    const uint16_t offset =
        pacman_video_board_tile_offset(
            tile_x,
            tile_y
        );

    if (
        offset >=
        PACMAN_VIDEO_BOARD_VIDEO_RAM_SIZE
    ) {
        return black;
    }

    const uint8_t tile =
        board->video_ram[offset];

    const uint8_t color =
        board->color_ram[offset] &
        0x1fu;

    const uint8_t pixel =
        pacman_video_board_tile_pixel(
            board,
            tile,
            pixel_x,
            pixel_y
        );

    return pacman_video_board_pixel_color(
        board,
        color,
        pixel
    );
}


static uint8_t sprite_palette_index(
    const PacmanVideoBoard *board,
    uint8_t color,
    uint8_t pixel
) {
    const uint16_t lookup =
        (uint16_t)(
            ((color & 0x1fu) << 2) |
            (pixel & 0x03u)
        );

    /*
     * Pac-Man sprites and tiles use the same base physical
     * palette. The +16 palette bank is a separate hardware
     * palette-bank feature; it is not selected merely because
     * the pixel belongs to a sprite.
     */
    return
        board->color_lookup_prom[lookup] &
        0x0fu;
}

static bool sample_sprite_copy(
    const PacmanVideoBoard *board,
    uint8_t sprite,
    uint8_t color,
    bool flip_x,
    bool flip_y,
    int sprite_x,
    int sprite_y,
    uint16_t x,
    uint16_t y,
    PacmanRgb *result
) {
    const int local_x =
        (int)x -
        sprite_x;

    const int local_y =
        (int)y -
        sprite_y;

    if (
        local_x < 0 ||
        local_x >= PACMAN_SPRITE_WIDTH ||
        local_y < 0 ||
        local_y >= PACMAN_SPRITE_HEIGHT
    ) {
        return false;
    }

    const uint8_t source_x =
        (uint8_t)(
            flip_x
                ? PACMAN_SPRITE_WIDTH - 1 - local_x
                : local_x
        );

    const uint8_t source_y =
        (uint8_t)(
            flip_y
                ? PACMAN_SPRITE_HEIGHT - 1 - local_y
                : local_y
        );

    const uint8_t pixel =
        pacman_video_board_sprite_pixel(
            board,
            sprite,
            source_x,
            source_y
        );

    const uint8_t palette_index =
        sprite_palette_index(
            board,
            color,
            pixel
        );

    /*
     * Sprite transparency is determined after the color
     * lookup PROM, not simply by graphics pen number.
     *
     * Any graphics pen that maps to physical palette color 0
     * is transparent. This is how the original Pac-Man color
     * table behaves and also makes color-code 0 sprites fully
     * transparent.
     */
    if (palette_index == 0u) {
        return false;
    }

    *result =
        pacman_video_board_palette_color(
            board,
            palette_index
        );

    return true;
}

static bool sample_sprite(
    const PacmanVideoBoard *board,
    uint8_t offset,
    uint16_t x,
    uint16_t y,
    PacmanRgb *result
) {
    const uint8_t attributes =
        board->sprite_attribute_ram[offset];

    const uint8_t color =
        board->sprite_attribute_ram[
            offset + 1u
        ] &
        0x1fu;

    const uint8_t sprite =
        (uint8_t)(
            attributes >> 2
        );

    const bool flip_x =
        (attributes & 0x01u) != 0u;

    const bool flip_y =
        (attributes & 0x02u) != 0u;

    const int sprite_x =
        272 -
        board->sprite_coordinate_ram[
            offset + 1u
        ];

    int sprite_y =
        (int)board->sprite_coordinate_ram[offset] -
        31;

    /*
     * Pac-Man hardware positioning quirk reproduced
     * by MAME for the low sprite entries.
     */
    if (offset <= 4u) {
        sprite_y++;
    }

    /*
     * Main sprite.
     */
    if (
        sample_sprite_copy(
            board,
            sprite,
            color,
            flip_x,
            flip_y,
            sprite_x,
            sprite_y,
            x,
            y,
            result
        )
    ) {
        return true;
    }

    /*
     * 8-bit horizontal coordinate wraps around.
     *
     * Historic MAME reproduces an additional flip-bit wiring
     * peculiarity for the low sprite entries on the wrapped copy.
     */
    bool wrap_flip_x =
        flip_x;

    bool wrap_flip_y =
        flip_y;

    if (offset <= 4u) {
        wrap_flip_x =
            (attributes & 0x02u) != 0u;

        wrap_flip_y =
            (attributes & 0x01u) != 0u;
    }

    return sample_sprite_copy(
        board,
        sprite,
        color,
        wrap_flip_x,
        wrap_flip_y,
        sprite_x - 256,
        sprite_y,
        x,
        y,
        result
    );
}

bool pacman_video_board_sprite_layer_pixel(
    const PacmanVideoBoard *board,
    uint16_t x,
    uint16_t y,
    PacmanRgb *color
) {
    if (
        board == NULL ||
        color == NULL ||
        x >= PACMAN_NATIVE_WIDTH ||
        y >= PACMAN_NATIVE_HEIGHT
    ) {
        return false;
    }

    /*
     * Original sprite hardware is visible only in the
     * central 256-pixel horizontal region.
     */
    if (
        x < 16u ||
        x > 271u
    ) {
        return false;
    }

    bool found = false;

    PacmanRgb current = {
        .r = 0,
        .g = 0,
        .b = 0
    };

    /*
     * Original draw priority.
     *
     * Later entries in this sequence overwrite earlier ones.
     */
    static const uint8_t order[] = {
        14u,
        12u,
        10u,
        8u,
        6u,
        4u,
        2u,
        0u
    };

    for (
        size_t i = 0;
        i < sizeof(order) / sizeof(order[0]);
        i++
    ) {
        PacmanRgb candidate;

        if (
            sample_sprite(
                board,
                order[i],
                x,
                y,
                &candidate
            )
        ) {
            current = candidate;
            found = true;
        }
    }

    if (found) {
        *color = current;
    }

    return found;
}

PacmanRgb pacman_video_board_output_pixel(
    const PacmanVideoBoard *board,
    uint16_t x,
    uint16_t y
) {
    PacmanRgb result =
        pacman_video_board_background_pixel(
            board,
            x,
            y
        );

    PacmanRgb sprite;

    if (
        pacman_video_board_sprite_layer_pixel(
            board,
            x,
            y,
            &sprite
        )
    ) {
        result = sprite;
    }

    return result;
}
