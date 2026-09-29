#include <cstdint>
#include <iostream>

#include "Vcrt_horizontal_bleed.h"
#include "verilated.h"

static uint16_t expected(
    uint16_t left,
    uint16_t center,
    uint16_t right
) {
    auto channel5 = [](uint16_t pixel, int shift) {
        return static_cast<uint16_t>((pixel >> shift) & 0x1F);
    };

    const uint16_t lr = channel5(left, 11);
    const uint16_t lb = channel5(left, 0);
    const uint16_t cr = channel5(center, 11);
    const uint16_t cb = channel5(center, 0);
    const uint16_t rr = channel5(right, 11);
    const uint16_t rb = channel5(right, 0);

    const uint16_t lg = (left >> 5) & 0x3F;
    const uint16_t cg = (center >> 5) & 0x3F;
    const uint16_t rg = (right >> 5) & 0x3F;

    const uint16_t ro = (lr * 8 + cr * 240 + rr * 8) >> 8;
    const uint16_t go = (lg * 8 + cg * 240 + rg * 8) >> 8;
    const uint16_t bo = (lb * 8 + cb * 240 + rb * 8) >> 8;

    return static_cast<uint16_t>(
        (ro << 11) |
        (go << 5) |
        bo
    );
}

static bool run_case(
    Vcrt_horizontal_bleed& dut,
    uint16_t left,
    uint16_t center,
    uint16_t right
) {
    dut.left_rgb565 = left;
    dut.center_rgb565 = center;
    dut.right_rgb565 = right;
    dut.eval();

    const uint16_t want = expected(left, center, right);

    if (dut.out_rgb565 != want) {
        std::cerr
            << "Mismatch:"
            << " left=0x" << std::hex << left
            << " center=0x" << center
            << " right=0x" << right
            << " expected=0x" << want
            << " got=0x" << dut.out_rgb565
            << "\n";
        return false;
    }

    return true;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vcrt_horizontal_bleed dut;

    if (!run_case(dut, 0x0000, 0xFFFF, 0x0000))
        return 1;

    if (!run_case(dut, 0xFFFF, 0xFFFF, 0xFFFF))
        return 1;

    if (!run_case(dut, 0xF800, 0x0000, 0x0000))
        return 1;

    if (!run_case(dut, 0x0000, 0x0000, 0x001F))
        return 1;

    if (!run_case(dut, 0x07E0, 0xFFFF, 0xF800))
        return 1;

    std::cout << "PASS: horizontal CRT bleed verified\n";
    return 0;
}
