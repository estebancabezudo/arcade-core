#ifndef ARCADE_CORE_HOST_PACMAN_ROM_LOADER_H
#define ARCADE_CORE_HOST_PACMAN_ROM_LOADER_H

#include <stdbool.h>

#include "pacman_romset.h"

/*
 * Load the ten Pac-Man CPU/video/sound ROM and PROM chips from one directory.
 *
 * Expected filenames:
 *
 *   pacman.6e
 *   pacman.6f
 *   pacman.6h
 *   pacman.6j
 *   pacman.5e
 *   pacman.5f
 *   82s123.7f
 *   82s126.4a
 *   82s126.1m
 *   82s126.3m
 */
bool host_load_pacman_romset(
    const char *directory,
    PacmanRomSet *romset
);

#endif
