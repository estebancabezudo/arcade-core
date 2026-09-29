#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pacman_machine.h"

static void build_program_rom(
    uint8_t *rom
) {
    memset(
        rom,
        0,
        PACMAN_PROGRAM_ROM_SIZE
    );

    /*
     * Visible tile coordinate:
     *
     * tile_x = 2
     * tile_y = 0
     *
     * physical RAM offset = 64
     *
     * Therefore:
     *
     * Video RAM address = 4000h + 40h = 4040h
     * Color RAM address = 4400h + 40h = 4440h
     *
     * Program:
     *
     * LD A,05h
     * LD (4040h),A
     *
     * LD A,03h
     * LD (4440h),A
     *
     * HALT
     */
    static const uint8_t program[] = {
        0x3e, 0x05,
        0x32, 0x40, 0x40,

        0x3e, 0x03,
        0x32, 0x40, 0x44,

        0x76
    };

    memcpy(
        rom,
        program,
        sizeof(program)
    );
}

int main(void)
{
    uint8_t program_rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    uint8_t tile_rom[
        PACMAN_TILE_ROM_SIZE
    ] = {0};

    uint8_t sprite_rom[
        PACMAN_SPRITE_ROM_SIZE
    ] = {0};

    uint8_t palette_prom[
        PACMAN_PALETTE_PROM_SIZE
    ] = {0};

    uint8_t lookup_prom[
        PACMAN_COLOR_LOOKUP_PROM_SIZE
    ] = {0};

    build_program_rom(
        program_rom
    );

    /*
     * Tile 5:
     *
     * local pixel (0,0) = graphics pen 1.
     */
    tile_rom[
        5u * 16u +
        8u
    ] = 0x08;

    /*
     * Palette entry 1 = full red.
     */
    palette_prom[1] = 0x07;

    /*
     * Color attribute 3, graphics pen 1
     * -> palette entry 1.
     */
    lookup_prom[
        3u * 4u +
        1u
    ] = 1;

    uint8_t sound_waveform_prom[
        PACMAN_AUDIO_WAVEFORM_PROM_SIZE
    ] = {0};

    uint8_t sound_timing_prom[
        PACMAN_AUDIO_TIMING_PROM_SIZE
    ] = {0};

    PacmanMachine machine;

    assert(
        pacman_machine_init(
            &machine,

            program_rom,
            sizeof(program_rom),

            tile_rom,
            sizeof(tile_rom),

            sprite_rom,
            sizeof(sprite_rom),

            palette_prom,
            sizeof(palette_prom),

            lookup_prom,
            sizeof(lookup_prom),

            sound_waveform_prom,
            sizeof(sound_waveform_prom),

            sound_timing_prom,
            sizeof(sound_timing_prom)
        )
    );

    /*
     * Before executing the ROM, the pixel is black.
     */
    PacmanRgb pixel =
        pacman_machine_video_pixel(
            &machine,
            16,
            0
        );

    assert(pixel.r == 0);
    assert(pixel.g == 0);
    assert(pixel.b == 0);

    uint32_t instructions = 0;

    while (!machine.cpu_board.cpu.halted) {
        const uint32_t cycles =
            pacman_machine_step(
                &machine
            );

        assert(cycles != 0);

        instructions++;

        assert(instructions < 100);
    }

    /*
     * We never touched Video RAM directly.
     *
     * The Z80 program reached it through:
     *
     * Z80
     *  -> PacmanCpuBoard address decoder
     *  -> PacmanVideoBoard
     */
    assert(
        machine.video_board.video_ram[64] ==
        5
    );

    assert(
        machine.video_board.color_ram[64] ==
        3
    );

    /*
     * And the hardware video pipeline converts those
     * bytes into the expected native RGB pixel.
     */
    pixel =
        pacman_machine_video_pixel(
            &machine,
            16,
            0
        );

    assert(pixel.r == 0xff);
    assert(pixel.g == 0x00);
    assert(pixel.b == 0x00);

    puts(
        "PASS Pac-Man machine CPU -> video end-to-end"
    );

    return 0;
}
