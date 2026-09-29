#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"

mode="${1:-all}"

source "$project_dir/scripts/common-build.sh"

lint() {
    verilator --lint-only -Wall --Wno-MULTITOP common/rtl/*.sv

    verilator \
        --lint-only \
        -Wall \
        --Wno-MULTITOP \
        --top-module tangnano20k_video_shell \
        common/rtl/*.sv \
        modular/platforms/fpga/tangnano20k/rtl/*.sv
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
        common/rtl/*.sv \
        "$project_dir/$testbench"

    "$build_dir/V$top"
}

video_system_test() {
    run_rtl_test \
        video_system \
        video_system \
        tests/rtl/video_system_tb.cpp
}

geometry_tests() {
    run_rtl_test \
        video_frame_address \
        video_frame_address \
        tests/rtl/video_frame_address_tb.cpp

    run_rtl_test \
        video_frame_packer \
        video_frame_packer \
        tests/rtl/video_frame_packer_tb.cpp

    run_rtl_test \
        video_capture_writer \
        video_capture_writer \
        tests/rtl/video_capture_writer_tb.cpp

    run_rtl_test \
        video_frame_line_fetcher \
        video_frame_line_fetcher \
        tests/rtl/video_frame_line_fetcher_tb.cpp

    run_rtl_test \
        hdmi_line_renderer \
        hdmi_line_renderer \
        tests/rtl/hdmi_line_renderer_tb.cpp

    run_rtl_test \
        hdmi_scanout_controller \
        hdmi_scanout_controller \
        tests/rtl/hdmi_scanout_controller_tb.cpp

    run_rtl_test \
        crt_scanline \
        crt_scanline \
        tests/rtl/crt_scanline_tb.cpp

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
        tests/rtl/video_frame_address_tb.cpp \
        "${pacman_source_parameters[@]}"

    run_rtl_test \
        video_frame_packer_pacman \
        video_frame_packer \
        tests/rtl/video_frame_packer_tb.cpp \
        "${pacman_source_parameters[@]}"

    run_rtl_test \
        video_capture_writer_pacman \
        video_capture_writer \
        tests/rtl/video_capture_writer_tb.cpp \
        "${pacman_source_parameters[@]}"

    run_rtl_test \
        video_frame_line_fetcher_pacman \
        video_frame_line_fetcher \
        tests/rtl/video_frame_line_fetcher_tb.cpp \
        "${pacman_source_parameters[@]}"

    run_rtl_test \
        hdmi_line_renderer_pacman \
        hdmi_line_renderer \
        tests/rtl/hdmi_line_renderer_tb.cpp \
        "${pacman_scaled_parameters[@]}"

    run_rtl_test \
        hdmi_scanout_controller_pacman \
        hdmi_scanout_controller \
        tests/rtl/hdmi_scanout_controller_tb.cpp \
        "${pacman_scaled_parameters[@]}"

    run_rtl_test \
        crt_scanline_pacman \
        crt_scanline \
        tests/rtl/crt_scanline_tb.cpp \
        -GSCALE=2 \
        -CFLAGS "-DTEST_SCALE=2"

    run_rtl_test \
        video_system_pacman \
        video_system \
        tests/rtl/video_system_tb.cpp \
        "${pacman_scaled_parameters[@]}"
}

rtl_tests() {
    geometry_tests

    run_rtl_test \
        async_fifo \
        async_fifo \
        tests/rtl/async_fifo_tb.cpp

    run_rtl_test \
        crt_horizontal_bleed \
        crt_horizontal_bleed \
        tests/rtl/crt_horizontal_bleed_tb.cpp

    run_rtl_test \
        crt_horizontal_bleed_stream \
        crt_horizontal_bleed_stream \
        tests/rtl/crt_horizontal_bleed_stream_tb.cpp

    run_rtl_test \
        crt_phosphor_mask \
        crt_phosphor_mask \
        tests/rtl/crt_phosphor_mask_tb.cpp

    run_rtl_test \
        video_720p_timing \
        video_720p_timing \
        tests/rtl/video_720p_timing_tb.cpp

    run_rtl_test \
        video_capture_pipeline \
        video_capture_pipeline \
        tests/rtl/video_capture_pipeline_tb.cpp

    run_rtl_test \
        video_frame_manager \
        video_frame_manager \
        tests/rtl/video_frame_manager_tb.cpp

    run_rtl_test \
        video_frame_manager_abort \
        video_frame_manager \
        tests/rtl/video_frame_manager_abort_tb.cpp

    run_rtl_test \
        video_memory_arbiter \
        video_memory_arbiter \
        tests/rtl/video_memory_arbiter_tb.cpp

    run_rtl_test \
        video_memory_command_queue \
        video_memory_command_queue \
        tests/rtl/video_memory_command_queue_tb.cpp

    run_rtl_test \
        video_memory_result_bridge \
        video_memory_result_bridge \
        tests/rtl/video_memory_result_bridge_tb.cpp

    run_rtl_test \
        video_framebuffer_pipeline \
        video_framebuffer_pipeline \
        tests/rtl/video_framebuffer_pipeline_tb.cpp
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
        find tests/common tests/modular \
            -type f \
            -name '*_test.c' \
            | sort
    )
}

case "$mode" in
    lint)
        lint
        ;;

    c)
        c_tests
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
            "Usage: $0 [lint|test|c|geometry|video-system|all]" \
            >&2
        exit 2
        ;;
esac
