#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pacman_machine.h"


typedef struct {
    int16_t samples[128];
    size_t count;
} AudioCapture;


static void capture_sample(
    void *context,
    int16_t sample
) {
    AudioCapture *capture = context;

    assert(
        capture->count <
        sizeof(capture->samples) /
            sizeof(capture->samples[0])
    );

    capture->samples[capture->count++] =
        sample;
}

int main(void)
{
    uint8_t program_rom[
        PACMAN_PROGRAM_ROM_SIZE
    ] = {0};

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

    uint8_t sound_waveform_prom[
        PACMAN_AUDIO_WAVEFORM_PROM_SIZE
    ];

    uint8_t sound_timing_prom[
        PACMAN_AUDIO_TIMING_PROM_SIZE
    ] = {0};

    /*
     * Constant waveform value 9:
     *
     * 9 - midpoint 8 = +1.
     */
    memset(
        sound_waveform_prom,
        9,
        sizeof(sound_waveform_prom)
    );

    /*
     * Z80 program:
     *
     *   LD A,1
     *   LD (5001h),A       ; sound enable
     *
     *   LD A,0
     *   LD (5045h),A       ; voice 0 waveform = 0
     *
     *   LD A,1
     *   LD (5054h),A       ; voice 0 frequency = 0x10000
     *
     *   LD A,1
     *   LD (5055h),A       ; voice 0 volume = 1
     *
     *   HALT
     */
    static const uint8_t program[] = {
        0x3e, 0x01,
        0x32, 0x01, 0x50,

        0x3e, 0x00,
        0x32, 0x45, 0x50,

        0x3e, 0x01,
        0x32, 0x54, 0x50,

        0x3e, 0x01,
        0x32, 0x55, 0x50,

        0x76
    };

    memcpy(
        program_rom,
        program,
        sizeof(program)
    );

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

    AudioCapture capture = {
        .samples = {0},
        .count = 0
    };

    pacman_machine_set_audio_sink(
        &machine,
        capture_sample,
        &capture
    );

    uint64_t consumed_t_states = 0u;

    /*
     * Reset state must have sound disabled.
     */
    assert(
        !machine.audio_board.wsg.enabled
    );

    /*
     * Execute through HALT using the real Z80 core and
     * Pac-Man CPU-board address decoder.
     */
    for (unsigned i = 0; i < 9u; i++) {
        const uint32_t t_states =
            pacman_machine_step(
                &machine
            );

        assert(t_states > 0u);

        consumed_t_states +=
            t_states;
    }

    assert(
        capture.count ==
        consumed_t_states / 16u
    );

    assert(
        machine.audio_t_state_accumulator ==
        consumed_t_states % 16u
    );

    assert(
        machine.cpu_board.cpu.halted
    );

    assert(
        machine.audio_board.wsg.enabled
    );

    assert(
        machine.audio_board.wsg.registers[0x05] ==
        0u
    );

    assert(
        machine.audio_board.wsg.registers[0x14] ==
        1u
    );

    assert(
        machine.audio_board.wsg.registers[0x15] ==
        1u
    );

    assert(
        machine.audio_board.wsg.voices[0].frequency ==
        UINT32_C(0x10000)
    );

    assert(
        machine.audio_board.wsg.voices[0].volume ==
        1u
    );

    /*
     * Constant +1 waveform at volume 1:
     *
     * 32767 / (128 * 3) = 85.
     */
    const uint32_t counter_before_manual_sample =
        machine.audio_board.wsg.voices[0].counter;

    assert(
        pacman_audio_board_next_sample(
            &machine.audio_board
        ) ==
        85
    );

    assert(
        machine.audio_board.wsg.voices[0].counter ==
        counter_before_manual_sample +
            UINT32_C(0x10000)
    );

    puts(
        "PASS Pac-Man Z80 -> audio board -> Namco WSG"
    );

    return 0;
}
