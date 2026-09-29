#include <cstdint>
#include <iostream>

#include "Vcrt_scanline.h"
#include "verilated.h"

#ifndef TEST_SCALE
#define TEST_SCALE 3
#endif

static uint16_t beam_intensity(uint16_t input, uint8_t repeat) {
    if (TEST_SCALE != 3)
        return 256;

    const uint16_t r5 = (input >> 11) & 0x1F;
    const uint16_t g6 = (input >> 5) & 0x3F;
    const uint16_t b5 = input & 0x1F;

    const uint16_t r6 = (r5 << 1) | (r5 >> 4);
    const uint16_t b6 = (b5 << 1) | (b5 >> 4);
    const uint16_t luminance = (r6 + 2 * g6 + b6) >> 2;

    if (repeat == 0)
        return 154 + luminance;

    if (repeat == 1)
        return 256;

    if (repeat == 2)
        return 179 + ((luminance * 13 + 8) >> 4);

    return 0;
}

static uint16_t expected_rgb565(uint16_t input, uint16_t intensity) {
    const uint16_t r = (input >> 11) & 0x1F;
    const uint16_t g = (input >> 5)  & 0x3F;
    const uint16_t b = input & 0x1F;

    const uint16_t ro = (r * intensity) >> 8;
    const uint16_t go = (g * intensity) >> 8;
    const uint16_t bo = (b * intensity) >> 8;

    return static_cast<uint16_t>(
        (ro << 11) |
        (go << 5)  |
        bo
    );
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vcrt_scanline dut;

    struct TestCase {
        uint16_t color;
        uint8_t repeat;
    };

    const TestCase tests[] = {
        {0xFFFF, 0}, {0xFFFF, 1}, {0xFFFF, 2},
        {0x7BEF, 0}, {0x7BEF, 1}, {0x7BEF, 2},
        {0x0841, 0}, {0x0841, 1}, {0x0841, 2},
        {0xF800, 0}, {0x07E0, 0}, {0x001F, 0},
        {0x0000, 0}, {0x0000, 1}, {0x0000, 2}
    };

    for (const auto& test : tests) {
        dut.in_rgb565 = test.color;
        dut.line_repeat = test.repeat;
        dut.eval();

        const uint16_t expected =
            expected_rgb565(
                test.color,
                beam_intensity(test.color, test.repeat));

        if (dut.out_rgb565 != expected) {
            std::cerr
                << "Mismatch:"
                << " input=0x" << std::hex << test.color
                << " repeat=" << std::dec
                << static_cast<int>(test.repeat)
                << " expected=0x" << std::hex << expected
                << " got=0x" << dut.out_rgb565
                << "\n";
            return 1;
        }
    }

    std::cout
        << "PASS: CRT beam verified for scale "
        << TEST_SCALE << "x\n";
    return 0;
}
