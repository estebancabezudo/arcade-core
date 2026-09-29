#include "pacman_video_board.h"

void pacman_video_timing_reset(
    PacmanVideoTiming *timing)
{
    if (timing == NULL)
    {
        return;
    }

    timing->x = 0;
    timing->y = 0;

    timing->frame = 0;

    timing->hblank = false;
    timing->vblank = false;
}

void pacman_video_timing_advance(
    PacmanVideoTiming *timing,
    uint32_t pixel_clocks)
{
    if (timing == NULL)
    {
        return;
    }

    for (
        uint32_t i = 0;
        i < pixel_clocks;
        i++)
    {
        timing->x++;

        if (
            timing->x >=
            PACMAN_RASTER_WIDTH)
        {
            timing->x = 0;
            timing->y++;

            if (
                timing->y >=
                PACMAN_RASTER_HEIGHT)
            {
                timing->y = 0;
                timing->frame++;
            }
        }

        timing->hblank =
            timing->x >=
            PACMAN_VISIBLE_WIDTH;

        timing->vblank =
            timing->y >=
            PACMAN_VISIBLE_HEIGHT;
    }
}