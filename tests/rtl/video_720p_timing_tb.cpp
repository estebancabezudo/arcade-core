#include <cstdint>
#include <iostream>

#include "Vvideo_720p_timing.h"
#include "verilated.h"

static constexpr int H_ACTIVE = 1280;
static constexpr int H_FRONT  = 110;
static constexpr int H_SYNC   = 40;
static constexpr int H_BACK   = 220;
static constexpr int H_TOTAL  = H_ACTIVE + H_FRONT + H_SYNC + H_BACK;

static constexpr int V_ACTIVE = 720;
static constexpr int V_FRONT  = 5;
static constexpr int V_SYNC   = 5;
static constexpr int V_BACK   = 20;
static constexpr int V_TOTAL  = V_ACTIVE + V_FRONT + V_SYNC + V_BACK;

static void tick(Vvideo_720p_timing& dut) {
    dut.clk = 1;
    dut.eval();

    dut.clk = 0;
    dut.eval();
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vvideo_720p_timing dut;

    dut.clk = 0;
    dut.reset = 1;
    dut.eval();

    tick(dut);

    dut.reset = 0;
    dut.eval();

    std::uint64_t active_count = 0;
    std::uint64_t hsync_count = 0;
    std::uint64_t vsync_count = 0;
    std::uint64_t line_start_count = 0;
    std::uint64_t frame_start_count = 0;

    for (int y = 0; y < V_TOTAL; ++y) {
        for (int x = 0; x < H_TOTAL; ++x) {
            if (dut.x != x || dut.y != y) {
                std::cerr
                    << "Coordinate mismatch: expected "
                    << x << "," << y
                    << " got "
                    << dut.x << "," << dut.y << "\n";
                return 1;
            }

            const bool expected_active =
                x < H_ACTIVE && y < V_ACTIVE;

            const bool expected_hsync =
                x >= H_ACTIVE + H_FRONT &&
                x < H_ACTIVE + H_FRONT + H_SYNC;

            const bool expected_vsync =
                y >= V_ACTIVE + V_FRONT &&
                y < V_ACTIVE + V_FRONT + V_SYNC;

            if (dut.active != expected_active) {
                std::cerr << "active mismatch at "
                          << x << "," << y << "\n";
                return 1;
            }

            if (dut.hsync != expected_hsync) {
                std::cerr << "hsync mismatch at "
                          << x << "," << y << "\n";
                return 1;
            }

            if (dut.vsync != expected_vsync) {
                std::cerr << "vsync mismatch at "
                          << x << "," << y << "\n";
                return 1;
            }

            if (dut.line_start != (x == 0)) {
                std::cerr << "line_start mismatch at "
                          << x << "," << y << "\n";
                return 1;
            }

            if (dut.frame_start != (x == 0 && y == 0)) {
                std::cerr << "frame_start mismatch at "
                          << x << "," << y << "\n";
                return 1;
            }

            active_count += dut.active;
            hsync_count += dut.hsync;
            vsync_count += dut.vsync;
            line_start_count += dut.line_start;
            frame_start_count += dut.frame_start;

            tick(dut);
        }
    }

    if (dut.x != 0 || dut.y != 0) {
        std::cerr << "Frame did not wrap to 0,0\n";
        return 1;
    }

    if (active_count !=
        static_cast<std::uint64_t>(H_ACTIVE) * V_ACTIVE) {
        std::cerr << "Active pixel count mismatch\n";
        return 1;
    }

    if (hsync_count !=
        static_cast<std::uint64_t>(H_SYNC) * V_TOTAL) {
        std::cerr << "HSYNC duration mismatch\n";
        return 1;
    }

    if (vsync_count !=
        static_cast<std::uint64_t>(V_SYNC) * H_TOTAL) {
        std::cerr << "VSYNC duration mismatch\n";
        return 1;
    }

    if (line_start_count != V_TOTAL) {
        std::cerr << "Line count mismatch\n";
        return 1;
    }

    if (frame_start_count != 1) {
        std::cerr << "Frame start count mismatch\n";
        return 1;
    }

    std::cout << "PASS: 1280x720p60 timing verified\n";
    std::cout << "PASS: "
              << H_TOTAL << "x" << V_TOTAL
              << " total raster, "
              << active_count
              << " active pixels\n";

    return 0;
}
