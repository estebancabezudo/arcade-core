#ifndef ARCADE_CORE_PACMAN_VIDEO_BOARD_H
#define ARCADE_CORE_PACMAN_VIDEO_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    PACMAN_VIDEO_BOARD_VIDEO_RAM_SIZE = 0x0400,
    PACMAN_VIDEO_BOARD_COLOR_RAM_SIZE = 0x0400,

    PACMAN_VIDEO_BOARD_SPRITE_ATTRIBUTE_RAM_SIZE = 0x0010,
    PACMAN_VIDEO_BOARD_SPRITE_COORDINATE_RAM_SIZE = 0x0010,

    PACMAN_TILE_ROM_SIZE = 0x1000,
    PACMAN_SPRITE_ROM_SIZE = 0x1000,

    PACMAN_PALETTE_PROM_SIZE = 0x0020,
    PACMAN_COLOR_LOOKUP_PROM_SIZE = 0x0100,

    PACMAN_NATIVE_WIDTH = 288,
    PACMAN_NATIVE_HEIGHT = 224,

    PACMAN_TILE_WIDTH = 8,
    PACMAN_TILE_HEIGHT = 8,

    PACMAN_SPRITE_WIDTH = 16,
    PACMAN_SPRITE_HEIGHT = 16
};

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} PacmanRgb;

typedef struct
{
    uint8_t video_ram[PACMAN_VIDEO_BOARD_VIDEO_RAM_SIZE];

    uint8_t color_ram[PACMAN_VIDEO_BOARD_COLOR_RAM_SIZE];

    uint8_t sprite_attribute_ram[PACMAN_VIDEO_BOARD_SPRITE_ATTRIBUTE_RAM_SIZE];

    uint8_t sprite_coordinate_ram[PACMAN_VIDEO_BOARD_SPRITE_COORDINATE_RAM_SIZE];

    const uint8_t *tile_rom;
    const uint8_t *sprite_rom;

    const uint8_t *palette_prom;
    const uint8_t *color_lookup_prom;

    bool flip_screen;
} PacmanVideoBoard;

bool pacman_video_board_init(
    PacmanVideoBoard *board,

    const uint8_t *tile_rom,
    size_t tile_rom_size,

    const uint8_t *sprite_rom,
    size_t sprite_rom_size,

    const uint8_t *palette_prom,
    size_t palette_prom_size,

    const uint8_t *color_lookup_prom,
    size_t color_lookup_prom_size);

void pacman_video_board_reset(
    PacmanVideoBoard *board);

uint8_t pacman_video_board_video_ram_read(
    void *context,
    uint16_t offset);

void pacman_video_board_video_ram_write(
    void *context,
    uint16_t offset,
    uint8_t value);

uint8_t pacman_video_board_color_ram_read(
    void *context,
    uint16_t offset);

void pacman_video_board_color_ram_write(
    void *context,
    uint16_t offset,
    uint8_t value);

uint8_t pacman_video_board_sprite_attribute_read(
    void *context,
    uint16_t offset);

void pacman_video_board_sprite_attribute_write(
    void *context,
    uint16_t offset,
    uint8_t value);

void pacman_video_board_sprite_coordinate_write(
    void *context,
    uint16_t offset,
    uint8_t value);

void pacman_video_board_set_flip_screen(
    void *context,
    bool enabled);

uint8_t pacman_video_board_tile_pixel(
    const PacmanVideoBoard *board,
    uint8_t tile,
    uint8_t x,
    uint8_t y);

uint8_t pacman_video_board_sprite_pixel(
    const PacmanVideoBoard *board,
    uint8_t sprite,
    uint8_t x,
    uint8_t y);

/*
 * Translate a 5-bit Pac-Man color code plus a 2-bit graphics pixel
 * through the 82S126 color lookup PROM.
 */
uint8_t pacman_video_board_palette_index(
    const PacmanVideoBoard *board,
    uint8_t color,
    uint8_t pixel);

/*
 * Decode one physical palette PROM entry through the resistor network.
 */
PacmanRgb pacman_video_board_palette_color(
    const PacmanVideoBoard *board,
    uint8_t palette_index);

/*
 * Complete lookup: color code + 2-bit pixel -> RGB.
 */
PacmanRgb pacman_video_board_pixel_color(
    const PacmanVideoBoard *board,
    uint8_t color,
    uint8_t pixel);

/*
 * Translate a visible tile coordinate in the 36x28 Pac-Man
 * tilemap to its physical Video RAM / Color RAM offset.
 */
uint16_t pacman_video_board_tile_offset(
    uint8_t tile_x,
    uint8_t tile_y);

/*
 * Produce one background pixel from the original Pac-Man
 * tile hardware.
 *
 * Coordinates are in the native unrotated 288x224 raster.
 */
PacmanRgb pacman_video_board_background_pixel(
    const PacmanVideoBoard *board,
    uint16_t x,
    uint16_t y);

/*
 * Resolve the sprite layer at one native raster coordinate.
 *
 * Returns true when an opaque sprite pixel is present.
 */
bool pacman_video_board_sprite_layer_pixel(
    const PacmanVideoBoard *board,
    uint16_t x,
    uint16_t y,
    PacmanRgb *color);

/*
 * Final native Pac-Man video output:
 *
 * background tile layer + sprite layer.
 */
PacmanRgb pacman_video_board_output_pixel(
    const PacmanVideoBoard *board,
    uint16_t x,
    uint16_t y);

enum
{
    PACMAN_PIXEL_CLOCK_HZ = 6144000,

    PACMAN_RASTER_WIDTH = 384,
    PACMAN_RASTER_HEIGHT = 264,

    PACMAN_VISIBLE_WIDTH = 288,
    PACMAN_VISIBLE_HEIGHT = 224,

    PACMAN_PIXELS_PER_CPU_T_STATE = 2
};

typedef struct
{
    uint16_t x;
    uint16_t y;

    uint64_t frame;

    bool hblank;
    bool vblank;
} PacmanVideoTiming;

void pacman_video_timing_reset(
    PacmanVideoTiming *timing);

void pacman_video_timing_advance(
    PacmanVideoTiming *timing,
    uint32_t pixel_clocks);

#endif
