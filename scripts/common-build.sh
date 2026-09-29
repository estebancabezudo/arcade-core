#!/usr/bin/env bash
# Shared native source manifest. Source this file from the repository root.
common_cflags=(
    -std=c17
    -O2
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Icommon/devices/z80
    -Icommon/devices/namco_wsg
    -Icommon/boards/pacman_audio
    -Icommon/boards/pacman_cpu
    -Icommon/boards/pacman_video
    -Icommon/machines/pacman
    -Icommon/romsets/pacman
    -Imodular/platforms/host
)

common_c_sources=(
    common/devices/z80/z80.c
    common/devices/namco_wsg/namco_wsg.c
    common/boards/pacman_audio/pacman_audio_board.c
    common/boards/pacman_cpu/pacman_cpu_board.c
    common/boards/pacman_video/pacman_video_board.c
    common/boards/pacman_video/pacman_video_timing.c
    common/machines/pacman/pacman_machine.c
    common/romsets/pacman/pacman_romset.c
    modular/platforms/host/file_loader.c
    modular/platforms/host/pacman_rom_loader.c
)
