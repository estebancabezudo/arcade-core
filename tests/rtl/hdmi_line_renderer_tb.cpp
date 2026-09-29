#include <cstdint>
#include <iostream>

#include "Vhdmi_line_renderer.h"
#include "verilated.h"

static constexpr int OUT_W = 1280;
static constexpr int OUT_H = 720;

#ifndef TEST_SOURCE_WIDTH
#define TEST_SOURCE_WIDTH 320
#endif

#ifndef TEST_SOURCE_HEIGHT
#define TEST_SOURCE_HEIGHT 240
#endif

#ifndef TEST_SCALE
#define TEST_SCALE 3
#endif

static constexpr int SOURCE_W = TEST_SOURCE_WIDTH;
static constexpr int SOURCE_H = TEST_SOURCE_HEIGHT;
static constexpr int SCALE = TEST_SCALE;
static constexpr int CONTENT_W = SOURCE_W * SCALE;
static constexpr int CONTENT_H = SOURCE_H * SCALE;
static constexpr int X_START = (OUT_W - CONTENT_W) / 2;
static constexpr int Y_START = (OUT_H - CONTENT_H) / 2;

static uint16_t source_pixel(int source_y, int source_x) {
    return static_cast<uint16_t>(
        ((source_y * 0x1234u) ^
         (source_x * 0x01F3u) ^
         0x5AA5u) & 0xFFFFu
    );
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vhdmi_line_renderer dut;

    dut.cache_valid = 1;
    dut.cache_rgb565 = 0;

    std::uint64_t content_count = 0;
    int release_count = 0;

    for (int y = 0; y < OUT_H; ++y) {
        for (int x = 0; x < OUT_W; ++x) {
            dut.timing_active = 1;
            dut.timing_x = x;
            dut.timing_y = y;

            /*
             * First evaluation determines which cached source pixel
             * the renderer wants.
             */
            dut.cache_rgb565 = 0;
            dut.eval();

            const int expected_source_y = (y - Y_START) / SCALE;

            if (dut.content_active) {
                const int expected_content_x = x - X_START;
                const int expected_source_x = expected_content_x / SCALE;

                if (dut.cache_read_x != expected_source_x) {
                    std::cerr
                        << "cache_read_x mismatch at "
                        << x << "," << y
                        << ": expected "
                        << expected_source_x
                        << ", got "
                        << dut.cache_read_x
                        << "\n";
                    return 1;
                }

                dut.cache_rgb565 =
                    source_pixel(expected_source_y, expected_source_x);

                /*
                 * Re-evaluate after providing the requested cache pixel.
                 */
                dut.eval();
            }

            const bool expected_content =
                x >= X_START &&
                x < X_START + CONTENT_W &&
                y >= Y_START &&
                y < Y_START + CONTENT_H;

            if (dut.content_active != expected_content) {
                std::cerr
                    << "content_active mismatch at "
                    << x << "," << y << "\n";
                return 1;
            }

            if (expected_content) {
                ++content_count;

                const int content_x = x - X_START;
                const int expected_source_x = content_x / SCALE;

                const int expected_h_repeat = content_x % SCALE;
                const int expected_v_repeat = (y - Y_START) % SCALE;

                const uint16_t expected_rgb =
                    source_pixel(expected_source_y, expected_source_x);

                if (dut.content_x != content_x) {
                    std::cerr
                        << "content_x mismatch at "
                        << x << "," << y << "\n";
                    return 1;
                }

                if (dut.horizontal_repeat != expected_h_repeat) {
                    std::cerr
                        << "horizontal_repeat mismatch at "
                        << x << "," << y
                        << ": expected "
                        << expected_h_repeat
                        << ", got "
                        << static_cast<int>(dut.horizontal_repeat)
                        << "\n";
                    return 1;
                }

                if (dut.vertical_repeat != expected_v_repeat) {
                    std::cerr
                        << "vertical_repeat mismatch at "
                        << x << "," << y
                        << ": expected "
                        << expected_v_repeat
                        << ", got "
                        << static_cast<int>(dut.vertical_repeat)
                        << "\n";
                    return 1;
                }

                if (dut.out_rgb565 != expected_rgb) {
                    std::cerr
                        << "Pixel mismatch at "
                        << x << "," << y
                        << ": source "
                        << expected_source_x << ","
                        << expected_source_y
                        << "\n";
                    return 1;
                }
            } else {
                if (dut.out_rgb565 != 0x0000) {
                    std::cerr
                        << "Border not black at "
                        << x << "," << y << "\n";
                    return 1;
                }
            }

            const bool expected_release =
                expected_content &&
                (x == X_START + CONTENT_W - 1) &&
                (((y - Y_START) % SCALE) == SCALE - 1);

            if (dut.release_line != expected_release) {
                std::cerr
                    << "release_line mismatch at "
                    << x << "," << y << "\n";
                return 1;
            }

            if (dut.release_line)
                ++release_count;
        }
    }

    const std::uint64_t expected_content_count =
        static_cast<std::uint64_t>(CONTENT_W) * CONTENT_H;

    if (content_count != expected_content_count) {
        std::cerr
            << "Content pixel count mismatch: expected "
            << expected_content_count
            << ", got "
            << content_count
            << "\n";
        return 1;
    }

    if (release_count != SOURCE_H) {
        std::cerr
            << "release_line count mismatch: expected "
            << SOURCE_H
            << ", got "
            << release_count
            << "\n";
        return 1;
    }

    /*
     * Blanking must always produce black and no release.
     */
    dut.timing_active = 0;
    dut.timing_x = X_START + CONTENT_W - 1;
    dut.timing_y = Y_START + SCALE - 1;
    dut.cache_rgb565 = 0xFFFF;
    dut.eval();

    if (
        dut.content_active ||
        dut.out_rgb565 != 0x0000 ||
        dut.release_line
    ) {
        std::cerr << "Blanking behaviour incorrect\n";
        return 1;
    }

    std::cout
        << "PASS: HDMI " << SCALE
        << "x line renderer verified\n";
    std::cout
        << "PASS: " << SOURCE_W << "x" << SOURCE_H
        << " -> " << CONTENT_W << "x" << CONTENT_H
        << " centred mapping verified\n";
    std::cout
        << "PASS: " << SOURCE_H
        << " source-line releases verified\n";

    return 0;
}
