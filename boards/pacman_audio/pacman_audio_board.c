#include "pacman_audio_board.h"

#include <string.h>

bool pacman_audio_board_init(
    PacmanAudioBoard *board,

    const uint8_t *waveform_prom,
    size_t waveform_prom_size,

    const uint8_t *timing_prom,
    size_t timing_prom_size
) {
    if (
        board == NULL ||
        timing_prom == NULL ||
        timing_prom_size !=
            PACMAN_AUDIO_TIMING_PROM_SIZE
    ) {
        return false;
    }

    memset(
        board,
        0,
        sizeof(*board)
    );

    if (
        !namco_wsg_init(
            &board->wsg,
            waveform_prom,
            waveform_prom_size
        )
    ) {
        return false;
    }

    board->timing_prom =
        timing_prom;

    board->timing_prom_size =
        timing_prom_size;

    pacman_audio_board_reset(
        board
    );

    return true;
}

void pacman_audio_board_reset(
    PacmanAudioBoard *board
) {
    if (board == NULL) {
        return;
    }

    namco_wsg_reset(
        &board->wsg
    );

    /*
     * The Pac-Man output latch resets with sound disabled.
     */
    namco_wsg_set_enabled(
        &board->wsg,
        false
    );
}

void pacman_audio_board_write_register(
    PacmanAudioBoard *board,
    uint16_t offset,
    uint8_t value
) {
    if (board == NULL) {
        return;
    }

    namco_wsg_write_register(
        &board->wsg,
        offset,
        value
    );
}

void pacman_audio_board_set_enabled(
    PacmanAudioBoard *board,
    bool enabled
) {
    if (board == NULL) {
        return;
    }

    namco_wsg_set_enabled(
        &board->wsg,
        enabled
    );
}

int16_t pacman_audio_board_next_sample(
    PacmanAudioBoard *board
) {
    if (board == NULL) {
        return 0;
    }

    return namco_wsg_next_sample(
        &board->wsg
    );
}
