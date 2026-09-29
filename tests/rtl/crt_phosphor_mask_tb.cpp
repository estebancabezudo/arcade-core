#include <cstdint>
#include <iostream>

#include "Vcrt_phosphor_mask.h"
#include "verilated.h"

static uint16_t expected_rgb565(
    uint16_t input,
    uint16_t red_intensity,
    uint16_t green_intensity,
    uint16_t blue_intensity
) {
    const uint16_t r = (input >> 11) & 0x1F;
    const uint16_t g = (input >> 5) & 0x3F;
    const uint16_t b = input & 0x1F;

    const uint16_t ro = (r * red_intensity) >> 8;
    const uint16_t go = (g * green_intensity) >> 8;
    const uint16_t bo = (b * blue_intensity) >> 8;

    return static_cast<uint16_t>(
        (ro << 11) |
        (go << 5) |
        bo
    );
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vcrt_phosphor_mask dut;

    constexpr uint16_t WHITE = 0xFFFF;

    struct TestCase {
        uint16_t x;
        uint16_t r;
        uint16_t g;
        uint16_t b;
    };

    const TestCase tests[] = {
        {0, 256, 241, 241},
        {1, 241, 256, 241},
        {2, 241, 241, 256},
        {3, 256, 241, 241},
        {4, 241, 256, 241},
        {5, 241, 241, 256}
    };

    for (const auto& test : tests) {
        dut.in_rgb565 = WHITE;
        dut.x = test.x;
        dut.eval();

        const uint16_t expected = expected_rgb565(
            WHITE,
            test.r,
            test.g,
            test.b
        );

        if (dut.out_rgb565 != expected) {
            std::cerr
                << "Mismatch at x=" << test.x
                << ": expected 0x" << std::hex << expected
                << ", got 0x" << dut.out_rgb565
                << "\n";
            return 1;
        }
    }

    std::cout << "PASS: RGB phosphor mask verified\n";
    return 0;
}
