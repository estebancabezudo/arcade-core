#ifndef ARCADE_CORE_STANDALONE_PLATFORM_H
#define ARCADE_CORE_STANDALONE_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    PACMAN_STANDALONE_WIDTH = 224,
    PACMAN_STANDALONE_HEIGHT = 288,
    PACMAN_STANDALONE_AUDIO_RATE = 48000
};

typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;

    bool coin;
    bool start1;
    bool start2;
} PacmanStandaloneInput;

/*
 * Initialize the target hardware.
 *
 * Return false when the platform cannot start.
 */
bool platform_init(void);

/*
 * Present one complete Pac-Man frame.
 *
 * Pixels are RGB565 in display orientation:
 * 224 x 288.
 */
void platform_video_present(
    const uint16_t *pixels,
    size_t pixel_count
);

/*
 * Submit signed 16-bit mono PCM at 48 kHz.
 */
void platform_audio_write(
    const int16_t *samples,
    size_t sample_count
);

/*
 * Read the current cabinet controls.
 */
PacmanStandaloneInput platform_input_read(void);

/*
 * Monotonic platform timer in microseconds.
 */
uint64_t platform_time_us(void);

/*
 * Optional low-power / wait hook while the core waits
 * for the next emulation deadline.
 */
void platform_idle(void);

#endif
