#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"

mode="${1:-all}"

common_cflags=(
    -std=c17
    -O2
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Idevices/z80
    -Idevices/namco_wsg
    -Iboards/pacman_audio
    -Iboards/pacman_cpu
    -Iboards/pacman_video
    -Imachines/pacman
    -Iromsets/pacman
    -Iplatform/host
)

common_c_sources=(
    devices/z80/z80.c
    devices/namco_wsg/namco_wsg.c
    boards/pacman_audio/pacman_audio_board.c
    boards/pacman_cpu/pacman_cpu_board.c
    boards/pacman_video/pacman_video_board.c
    boards/pacman_video/pacman_video_timing.c
    machines/pacman/pacman_machine.c
    romsets/pacman/pacman_romset.c
    platform/host/file_loader.c
    platform/host/pacman_rom_loader.c
)

lint() {
    verilator --lint-only -Wall --Wno-MULTITOP rtl/*.sv

    verilator \
        --lint-only \
        -Wall \
        --Wno-MULTITOP \
        --top-module tangnano20k_video_shell \
        rtl/*.sv \
        fpga/tangnano20k/rtl/*.sv
}

run_rtl_test() {
    local build_name="$1"
    local top="$2"
    local testbench="$3"
    local build_dir="build/$build_name"
    shift 3

    verilator \
        --cc \
        --exe \
        --build \
        -Wall \
        --Wno-MULTITOP \
        --Mdir "$build_dir" \
        --top-module "$top" \
        "$@" \
        rtl/*.sv \
        "$testbench"

    "$build_dir/V$top"
}

video_system_test() {
    run_rtl_test \
        video_system \
        video_system \
        sim/video_system_tb.cpp
}

geometry_tests() {
    run_rtl_test \
        video_frame_address \
        video_frame_address \
        sim/video_frame_address_tb.cpp

    run_rtl_test \
        video_frame_packer \
        video_frame_packer \
        sim/video_frame_packer_tb.cpp

    run_rtl_test \
        video_capture_writer \
        video_capture_writer \
        sim/video_capture_writer_tb.cpp

    run_rtl_test \
        video_frame_line_fetcher \
        video_frame_line_fetcher \
        sim/video_frame_line_fetcher_tb.cpp

    run_rtl_test \
        hdmi_line_renderer \
        hdmi_line_renderer \
        sim/hdmi_line_renderer_tb.cpp

    run_rtl_test \
        hdmi_scanout_controller \
        hdmi_scanout_controller \
        sim/hdmi_scanout_controller_tb.cpp

    run_rtl_test \
        crt_scanline \
        crt_scanline \
        sim/crt_scanline_tb.cpp

    video_system_test

    local pacman_source_parameters=(
        -GSOURCE_WIDTH=224
        -GSOURCE_HEIGHT=288
        -CFLAGS
        "-DTEST_SOURCE_WIDTH=224 -DTEST_SOURCE_HEIGHT=288 -DTEST_SCALE=2"
    )

    local pacman_scaled_parameters=(
        -GSOURCE_WIDTH=224
        -GSOURCE_HEIGHT=288
        -GSCALE=2
        -CFLAGS
        "-DTEST_SOURCE_WIDTH=224 -DTEST_SOURCE_HEIGHT=288 -DTEST_SCALE=2"
    )

    run_rtl_test \
        video_frame_address_pacman \
        video_frame_address \
        sim/video_frame_address_tb.cpp \
        "${pacman_source_parameters[@]}"

    run_rtl_test \
        video_frame_packer_pacman \
        video_frame_packer \
        sim/video_frame_packer_tb.cpp \
        "${pacman_source_parameters[@]}"

    run_rtl_test \
        video_capture_writer_pacman \
        video_capture_writer \
        sim/video_capture_writer_tb.cpp \
        "${pacman_source_parameters[@]}"

    run_rtl_test \
        video_frame_line_fetcher_pacman \
        video_frame_line_fetcher \
        sim/video_frame_line_fetcher_tb.cpp \
        "${pacman_source_parameters[@]}"

    run_rtl_test \
        hdmi_line_renderer_pacman \
        hdmi_line_renderer \
        sim/hdmi_line_renderer_tb.cpp \
        "${pacman_scaled_parameters[@]}"

    run_rtl_test \
        hdmi_scanout_controller_pacman \
        hdmi_scanout_controller \
        sim/hdmi_scanout_controller_tb.cpp \
        "${pacman_scaled_parameters[@]}"

    run_rtl_test \
        crt_scanline_pacman \
        crt_scanline \
        sim/crt_scanline_tb.cpp \
        -GSCALE=2 \
        -CFLAGS "-DTEST_SCALE=2"

    run_rtl_test \
        video_system_pacman \
        video_system \
        sim/video_system_tb.cpp \
        "${pacman_scaled_parameters[@]}"
}

rtl_tests() {
    geometry_tests

    run_rtl_test \
        async_fifo \
        async_fifo \
        sim/async_fifo_tb.cpp

    run_rtl_test \
        crt_horizontal_bleed \
        crt_horizontal_bleed \
        sim/crt_horizontal_bleed_tb.cpp

    run_rtl_test \
        crt_horizontal_bleed_stream \
        crt_horizontal_bleed_stream \
        sim/crt_horizontal_bleed_stream_tb.cpp

    run_rtl_test \
        crt_phosphor_mask \
        crt_phosphor_mask \
        sim/crt_phosphor_mask_tb.cpp

    run_rtl_test \
        video_720p_timing \
        video_720p_timing \
        sim/video_720p_timing_tb.cpp

    run_rtl_test \
        video_capture_pipeline \
        video_capture_pipeline \
        sim/video_capture_pipeline_tb.cpp

    run_rtl_test \
        video_frame_manager \
        video_frame_manager \
        sim/video_frame_manager_tb.cpp

    run_rtl_test \
        video_frame_manager_abort \
        video_frame_manager \
        sim/video_frame_manager_abort_tb.cpp

    run_rtl_test \
        video_memory_arbiter \
        video_memory_arbiter \
        sim/video_memory_arbiter_tb.cpp

    run_rtl_test \
        video_memory_command_queue \
        video_memory_command_queue \
        sim/video_memory_command_queue_tb.cpp

    run_rtl_test \
        video_memory_result_bridge \
        video_memory_result_bridge \
        sim/video_memory_result_bridge_tb.cpp

    run_rtl_test \
        video_framebuffer_pipeline \
        video_framebuffer_pipeline \
        sim/video_framebuffer_pipeline_tb.cpp
}

c_tests() {
    local build_dir="build/c_tests"
    local object_dir="$build_dir/objects"
    local library="$build_dir/libarcade_core_test.a"

    rm -rf "$build_dir"
    mkdir -p "$object_dir" build/host

    local objects=()

    for source in "${common_c_sources[@]}"; do
        local object="$object_dir/$(basename "${source%.c}").o"

        cc \
            "${common_cflags[@]}" \
            -c "$source" \
            -o "$object"

        objects+=("$object")
    done

    ar rcs "$library" "${objects[@]}"

    local test
    while IFS= read -r test; do
        local relative="${test#tests/}"
        local name="${relative%.c}"
        name="${name//\//_}"

        local binary="$build_dir/$name"

        cc \
            "${common_cflags[@]}" \
            "$test" \
            "$library" \
            -o "$binary"

        "$binary"
    done < <(
        find tests \
            -type f \
            -name '*_test.c' \
            | sort
    )
}

case "$mode" in
    lint)
        lint
        ;;

    test)
        lint
        rtl_tests
        c_tests
        ;;

    video-system)
        video_system_test
        ;;

    geometry)
        lint
        geometry_tests
        ;;

    all)
        lint
        rtl_tests
        c_tests
        ;;

    *)
        echo \
            "Usage: $0 [lint|test|geometry|video-system|all]" \
            >&2
        exit 2
        ;;
esac
