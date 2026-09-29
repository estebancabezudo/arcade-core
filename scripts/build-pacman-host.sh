#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"

pkg-config --exists sdl2

mkdir -p \
    build/host \
    build/pacman_host_c

source "$project_dir/scripts/common-build.sh"

objects=()

for source in "${common_c_sources[@]}"; do
    object="build/pacman_host_c/$(basename "${source%.c}").o"

    cc \
        "${common_cflags[@]}" \
        -c "$source" \
        -o "$object"

    objects+=("$object")
done

ar rcs \
    build/pacman_host_c/libpacman_host.a \
    "${objects[@]}"

cc \
    "${common_cflags[@]}" \
    modular/platforms/host/pacman_sim.c \
    build/pacman_host_c/libpacman_host.a \
    -o build/host/pacman-sim \
    $(pkg-config --cflags --libs sdl2)

cc \
    "${common_cflags[@]}" \
    modular/platforms/host/pacman_hdmi_sim.c \
    build/pacman_host_c/libpacman_host.a \
    -o build/host/pacman-hdmi-sim \
    $(pkg-config --cflags --libs sdl2)

rm -rf build/pacman_video_system_sdl

verilator \
    --cc \
    --exe \
    --build \
    -Wall \
    --Wno-MULTITOP \
    --top-module video_system \
    -Mdir build/pacman_video_system_sdl \
    -GSOURCE_WIDTH=224 \
    -GSOURCE_HEIGHT=288 \
    -GSCALE=2 \
    -CFLAGS "-std=c++17 -O2 $(pkg-config --cflags sdl2) -I$project_dir/common/devices/z80 -I$project_dir/common/devices/namco_wsg -I$project_dir/common/boards/pacman_audio -I$project_dir/common/boards/pacman_cpu -I$project_dir/common/boards/pacman_video -I$project_dir/common/machines/pacman -I$project_dir/common/romsets/pacman -I$project_dir/modular/platforms/host" \
    -LDFLAGS "$project_dir/build/pacman_host_c/libpacman_host.a $(pkg-config --libs sdl2)" \
    "$project_dir/modular/platforms/host/pacman_video_system_sdl.cpp" \
    common/rtl/*.sv

ln -sfn \
    ../pacman_video_system_sdl/Vvideo_system \
    build/host/pacman-video-system-sdl

printf '%s\n' \
    "Built Pac-Man host targets:" \
    "  build/host/pacman-sim" \
    "  build/host/pacman-hdmi-sim" \
    "  build/host/pacman-video-system-sdl"
