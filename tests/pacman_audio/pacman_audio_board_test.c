#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pacman_audio_board.h"

static void make_waveform_prom(
    uint8_t *prom
) {
    /*
     * Constant +1 waveform after the WSG's -8 midpoint.
     */
    memset(
        prom,
        9,
        PACMAN_AUDIO_WAVEFORM_PROM_SIZE
    );
}

static void test_initialization_and_reset(void)
{
    uint8_t waveform_prom[
        PACMAN_AUDIO_WAVEFORM_PROM_SIZE
    ];

    uint8_t timing_prom[
        PACMAN_AUDIO_TIMING_PROM_SIZE
    ];

    make_waveform_prom(
        waveform_prom
    );

    memset(
        timing_prom,
        0x5a,
        sizeof(timing_prom)
    );

    PacmanAudioBoard board;

    assert(
        pacman_audio_board_init(
            &board,

            waveform_prom,
            sizeof(waveform_prom),

            timing_prom,
            sizeof(timing_prom)
        )
    );

    assert(
        board.wsg.waveform_prom ==
        waveform_prom
    );

    assert(
        board.timing_prom ==
        timing_prom
    );

    assert(
        board.timing_prom_size ==
        sizeof(timing_prom)
    );

    /*
     * Pac-Man starts with the sound-enable latch cleared.
     */
    assert(!board.wsg.enabled);

    assert(
        !pacman_audio_board_init(
            &board,

            waveform_prom,
            sizeof(waveform_prom),

            timing_prom,
            sizeof(timing_prom) - 1u
        )
    );
}

static void test_registers_enable_and_pcm(void)
{
    uint8_t waveform_prom[
        PACMAN_AUDIO_WAVEFORM_PROM_SIZE
    ];

    uint8_t timing_prom[
        PACMAN_AUDIO_TIMING_PROM_SIZE
    ] = {0};

    make_waveform_prom(
        waveform_prom
    );

    PacmanAudioBoard board;

    assert(
        pacman_audio_board_init(
            &board,

            waveform_prom,
            sizeof(waveform_prom),

            timing_prom,
            sizeof(timing_prom)
        )
    );

    /*
     * Program voice 0 while sound is disabled.
     */
    pacman_audio_board_write_register(
        &board,
        0x14,
        1
    );

    pacman_audio_board_write_register(
        &board,
        0x15,
        1
    );

    assert(
        board.wsg.voices[0].frequency ==
        UINT32_C(0x10000)
    );

    assert(
        board.wsg.voices[0].volume ==
        1u
    );

    const uint32_t counter_before =
        board.wsg.voices[0].counter;

    assert(
        pacman_audio_board_next_sample(
            &board
        ) ==
        0
    );

    assert(
        board.wsg.voices[0].counter ==
        counter_before
    );

    pacman_audio_board_set_enabled(
        &board,
        true
    );

    assert(board.wsg.enabled);

    assert(
        pacman_audio_board_next_sample(
            &board
        ) !=
        0
    );

    assert(
        board.wsg.voices[0].counter ==
        counter_before +
            UINT32_C(0x10000)
    );

    pacman_audio_board_reset(
        &board
    );

    assert(!board.wsg.enabled);

    assert(
        board.wsg.voices[0].frequency ==
        0u
    );

    assert(
        board.wsg.voices[0].volume ==
        0u
    );

    assert(
        board.wsg.voices[0].counter ==
        0u
    );
}

int main(void)
{
    test_initialization_and_reset();
    test_registers_enable_and_pcm();

    puts(
        "PASS Pac-Man audio board"
    );

    return 0;
}
