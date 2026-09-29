#ifndef ARCADE_CORE_PACMAN_AUDIO_BOARD_H
#define ARCADE_CORE_PACMAN_AUDIO_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "namco_wsg.h"

enum {
    PACMAN_AUDIO_WAVEFORM_PROM_SIZE =
        NAMCO_WSG_WAVEFORM_PROM_SIZE,

    PACMAN_AUDIO_TIMING_PROM_SIZE = 0x0100
};

typedef struct {
    NamcoWsg wsg;

    /*
     * Retained because it belongs to the physical Pac-Man
     * sound board ROM set. The current WSG model does not
     * consume this timing PROM directly.
     */
    const uint8_t *timing_prom;
    size_t timing_prom_size;
} PacmanAudioBoard;

bool pacman_audio_board_init(
    PacmanAudioBoard *board,

    const uint8_t *waveform_prom,
    size_t waveform_prom_size,

    const uint8_t *timing_prom,
    size_t timing_prom_size
);

void pacman_audio_board_reset(
    PacmanAudioBoard *board
);

void pacman_audio_board_write_register(
    PacmanAudioBoard *board,
    uint16_t offset,
    uint8_t value
);

void pacman_audio_board_set_enabled(
    PacmanAudioBoard *board,
    bool enabled
);

/*
 * Generate one mono PCM16 sample at the WSG internal
 * sample rate (192 kHz).
 */
int16_t pacman_audio_board_next_sample(
    PacmanAudioBoard *board
);

#endif
