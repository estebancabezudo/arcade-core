#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Vcrt_horizontal_bleed_stream.h"
#include "verilated.h"

struct OutputPixel {
    uint16_t rgb;
    uint16_t x;
    bool line_start;
    uint8_t line_repeat;
};

static void tick(Vcrt_horizontal_bleed_stream& dut) {
    dut.clk = 1;
    dut.eval();

    dut.clk = 0;
    dut.eval();
}

static uint16_t filter_pixel(
    uint16_t left,
    uint16_t center,
    uint16_t right
) {
    const uint16_t lr = (left >> 11) & 0x1F;
    const uint16_t lg = (left >> 5)  & 0x3F;
    const uint16_t lb = left & 0x1F;

    const uint16_t cr = (center >> 11) & 0x1F;
    const uint16_t cg = (center >> 5)  & 0x3F;
    const uint16_t cb = center & 0x1F;

    const uint16_t rr = (right >> 11) & 0x1F;
    const uint16_t rg = (right >> 5)  & 0x3F;
    const uint16_t rb = right & 0x1F;

    const uint16_t ro = (lr * 8 + cr * 240 + rr * 8) >> 8;
    const uint16_t go = (lg * 8 + cg * 240 + rg * 8) >> 8;
    const uint16_t bo = (lb * 8 + cb * 240 + rb * 8) >> 8;

    return static_cast<uint16_t>(
        (ro << 11) |
        (go << 5) |
        bo
    );
}

static void capture_output(
    Vcrt_horizontal_bleed_stream& dut,
    std::vector<OutputPixel>& output
) {
    if (!dut.out_valid)
        return;

    output.push_back({
        static_cast<uint16_t>(dut.out_rgb565),
        static_cast<uint16_t>(dut.out_x),
        static_cast<bool>(dut.out_line_start),
        static_cast<uint8_t>(dut.out_line_repeat)
    });
}

static void send_pixel(
    Vcrt_horizontal_bleed_stream& dut,
    std::vector<OutputPixel>& output,
    uint16_t rgb,
    uint16_t x,
    bool line_start,
    uint8_t line_repeat
) {
    dut.in_valid = 1;
    dut.in_rgb565 = rgb;
    dut.in_x = x;
    dut.in_line_start = line_start;
    dut.in_line_repeat = line_repeat;

    tick(dut);
    capture_output(dut, output);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vcrt_horizontal_bleed_stream dut;

    dut.clk = 0;
    dut.reset = 1;
    dut.in_valid = 0;
    dut.in_rgb565 = 0;
    dut.in_x = 0;
    dut.in_line_start = 0;
    dut.in_line_repeat = 0;
    dut.eval();

    tick(dut);

    dut.reset = 0;
    dut.eval();

    const std::array<uint16_t, 4> line0 = {
        0xF800, // red
        0x07E0, // green
        0x001F, // blue
        0xFFFF  // white
    };

    const std::array<uint16_t, 4> line1 = {
        0xFFE0, // yellow
        0x07FF, // cyan
        0xF81F, // magenta
        0x0000  // black
    };

    std::vector<OutputPixel> output;

    for (uint16_t x = 0; x < line0.size(); ++x) {
        send_pixel(
            dut,
            output,
            line0[x],
            x,
            x == 0,
            0
        );
    }

    /*
     * No gap here. This is the important transition.
     */
    for (uint16_t x = 0; x < line1.size(); ++x) {
        send_pixel(
            dut,
            output,
            line1[x],
            x,
            x == 0,
            1
        );
    }

    /*
     * One invalid cycle flushes the final pixel of line 1.
     */
    dut.in_valid = 0;
    dut.in_line_start = 0;
    tick(dut);
    capture_output(dut, output);

    /*
     * A second invalid cycle must produce nothing.
     */
    tick(dut);
    capture_output(dut, output);

    if (output.size() != 8) {
        std::cerr
            << "Expected 8 output pixels, got "
            << output.size()
            << "\n";
        return 1;
    }

    for (int line = 0; line < 2; ++line) {
        const auto& source = (line == 0) ? line0 : line1;

        for (int x = 0; x < 4; ++x) {
            const uint16_t left =
                (x == 0) ? 0x0000 : source[x - 1];

            const uint16_t center = source[x];

            const uint16_t right =
                (x == 3) ? 0x0000 : source[x + 1];

            const uint16_t expected_rgb =
                filter_pixel(left, center, right);

            const auto& actual = output[line * 4 + x];

            if (actual.rgb != expected_rgb) {
                std::cerr
                    << "Pixel mismatch at line "
                    << line << ", x=" << x
                    << ": expected 0x" << std::hex
                    << expected_rgb
                    << ", got 0x"
                    << actual.rgb
                    << "\n";
                return 1;
            }

            if (actual.x != x) {
                std::cerr
                    << "X mismatch at line "
                    << line << ": expected "
                    << x << ", got "
                    << actual.x << "\n";
                return 1;
            }

            if (actual.line_start != (x == 0)) {
                std::cerr
                    << "line_start mismatch at line "
                    << line << ", x=" << x << "\n";
                return 1;
            }

            if (actual.line_repeat != line) {
                std::cerr
                    << "line_repeat mismatch at line "
                    << line << ", x=" << x << "\n";
                return 1;
            }
        }
    }

    std::cout
        << "PASS: streaming horizontal bleed verified\n";

    std::cout
        << "PASS: adjacent lines remain isolated\n";

    return 0;
}
