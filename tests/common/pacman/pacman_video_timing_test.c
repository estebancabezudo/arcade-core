#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "pacman_video_board.h"

int main(void)
{
    PacmanVideoTiming timing;

    pacman_video_timing_reset(
        &timing);

    assert(timing.x == 0);
    assert(timing.y == 0);
    assert(timing.frame == 0);

    assert(!timing.hblank);
    assert(!timing.vblank);

    /*
     * End of visible portion of first line.
     */
    pacman_video_timing_advance(
        &timing,
        PACMAN_VISIBLE_WIDTH);

    assert(timing.x == 288);
    assert(timing.y == 0);
    assert(timing.hblank);
    assert(!timing.vblank);

    /*
     * Complete the horizontal blank.
     */
    pacman_video_timing_advance(
        &timing,
        PACMAN_RASTER_WIDTH -
            PACMAN_VISIBLE_WIDTH);

    assert(timing.x == 0);
    assert(timing.y == 1);
    assert(!timing.hblank);

    /*
     * Reach vertical blank.
     */
    pacman_video_timing_reset(
        &timing);

    pacman_video_timing_advance(
        &timing,
        PACMAN_RASTER_WIDTH *
            PACMAN_VISIBLE_HEIGHT);

    assert(timing.x == 0);
    assert(timing.y == 224);

    assert(timing.vblank);

    /*
     * Complete one exact frame.
     */
    pacman_video_timing_reset(
        &timing);

    pacman_video_timing_advance(
        &timing,
        PACMAN_RASTER_WIDTH *
            PACMAN_RASTER_HEIGHT);

    assert(timing.x == 0);
    assert(timing.y == 0);

    assert(timing.frame == 1);

    assert(!timing.hblank);
    assert(!timing.vblank);

    /*
     * One frame expressed in CPU T-states.
     *
     * 384 * 264 / 2 = 50688.
     */
    assert(
        (
            PACMAN_RASTER_WIDTH *
            PACMAN_RASTER_HEIGHT) /
            PACMAN_PIXELS_PER_CPU_T_STATE ==
        50688);

    puts(
        "PASS Pac-Man raster timing 384x264 @ 2 pixels/T-state");

    return 0;
}