#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pacman_machine.h"

static void init_machine(
    PacmanMachine *machine,
    uint8_t *program_rom
) {
    static uint8_t tile_rom[
        PACMAN_TILE_ROM_SIZE
    ];

    static uint8_t sprite_rom[
        PACMAN_SPRITE_ROM_SIZE
    ];

    static uint8_t palette_prom[
        PACMAN_PALETTE_PROM_SIZE
    ];

    static uint8_t lookup_prom[
        PACMAN_COLOR_LOOKUP_PROM_SIZE
    ];

    static uint8_t sound_waveform_prom[
        PACMAN_AUDIO_WAVEFORM_PROM_SIZE
    ];

    static uint8_t sound_timing_prom[
        PACMAN_AUDIO_TIMING_PROM_SIZE
    ];

    memset(
        tile_rom,
        0,
        sizeof(tile_rom)
    );

    memset(
        sprite_rom,
        0,
        sizeof(sprite_rom)
    );

    memset(
        palette_prom,
        0,
        sizeof(palette_prom)
    );

    memset(
        lookup_prom,
        0,
        sizeof(lookup_prom)
    );

    memset(
        sound_waveform_prom,
        0,
        sizeof(sound_waveform_prom)
    );

    memset(
        sound_timing_prom,
        0,
        sizeof(sound_timing_prom)
    );

    assert(
        pacman_machine_init(
            machine,

            program_rom,
            PACMAN_PROGRAM_ROM_SIZE,

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
}

static void test_instruction_advances_video_clock(void)
{
    uint8_t program_rom[
        PACMAN_PROGRAM_ROM_SIZE
    ] = {0};

    /*
     * LD BC,1234h = 10 T-states.
     */
    program_rom[0] = 0x01;
    program_rom[1] = 0x34;
    program_rom[2] = 0x12;

    PacmanMachine machine;

    init_machine(
        &machine,
        program_rom
    );

    assert(
        pacman_machine_step(
            &machine
        ) == 10
    );

    /*
     * 10 CPU T-states * 2 pixel clocks/T-state.
     */
    assert(machine.video_timing.x == 20);
    assert(machine.video_timing.y == 0);

    assert(
        machine.cpu_board.cpu.total_t_states ==
        10
    );
}

static void test_vblank_and_frame_timing(void)
{
    /*
     * All-zero ROM = continuous NOP instructions.
     */
    uint8_t program_rom[
        PACMAN_PROGRAM_ROM_SIZE
    ] = {0};

    PacmanMachine machine;

    init_machine(
        &machine,
        program_rom
    );

    /*
     * Enable the Pac-Man IRQ latch.
     */
    pacman_cpu_board_memory_write(
        &machine.cpu_board,
        0x5000,
        1
    );

    /*
     * Visible raster:
     *
     * 384 clocks/line
     * 224 visible lines
     * 2 pixel clocks/T-state
     *
     * = 43008 CPU T-states.
     */
    const uint64_t visible_t_states =
        (
            PACMAN_RASTER_WIDTH *
            PACMAN_VISIBLE_HEIGHT
        ) /
        PACMAN_PIXELS_PER_CPU_T_STATE;

    assert(visible_t_states == 43008);

    const uint64_t consumed_visible =
        pacman_machine_run(
            &machine,
            visible_t_states
        );

    assert(
        consumed_visible ==
        visible_t_states
    );

    assert(machine.video_timing.x == 0);
    assert(machine.video_timing.y == 224);

    assert(machine.video_timing.vblank);
    assert(machine.cpu_board.vblank);

    /*
     * VBlank rising edge asserted the external Z80 INT line.
     */
    assert(machine.cpu_board.cpu.irq_line);

    /*
     * Pac-Man software can clear IRQ through latch bit 0.
     */
    pacman_cpu_board_memory_write(
        &machine.cpu_board,
        0x5000,
        0
    );

    assert(!machine.cpu_board.cpu.irq_line);

    /*
     * Remaining 40 raster lines are VBlank:
     *
     * 384 * 40 / 2 = 7680 T-states.
     */
    const uint64_t vblank_t_states =
        (
            PACMAN_RASTER_WIDTH *
            (
                PACMAN_RASTER_HEIGHT -
                PACMAN_VISIBLE_HEIGHT
            )
        ) /
        PACMAN_PIXELS_PER_CPU_T_STATE;

    assert(vblank_t_states == 7680);

    const uint64_t consumed_vblank =
        pacman_machine_run(
            &machine,
            vblank_t_states
        );

    assert(
        consumed_vblank ==
        vblank_t_states
    );

    /*
     * Exact frame boundary.
     */
    assert(machine.video_timing.x == 0);
    assert(machine.video_timing.y == 0);

    assert(machine.video_timing.frame == 1);

    assert(!machine.video_timing.vblank);
    assert(!machine.cpu_board.vblank);

    /*
     * 43008 + 7680 = 50688 T-states/frame.
     */
    assert(
        machine.cpu_board.cpu.total_t_states ==
        50688
    );
}

int main(void)
{
    test_instruction_advances_video_clock();
    test_vblank_and_frame_timing();

    puts(
        "PASS Pac-Man synchronized CPU/video raster and VBlank IRQ"
    );

    return 0;
}
