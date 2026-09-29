#include <cstdint>
#include <iostream>
#include <vector>

#include "Vvideo_frame_address.h"
#include "verilated.h"

#ifndef TEST_SOURCE_WIDTH
#define TEST_SOURCE_WIDTH 320
#endif

#ifndef TEST_SOURCE_HEIGHT
#define TEST_SOURCE_HEIGHT 240
#endif

static constexpr int WIDTH = TEST_SOURCE_WIDTH;
static constexpr int HEIGHT = TEST_SOURCE_HEIGHT;
static constexpr int SLOTS = 4;

static constexpr int WORDS_PER_ROW = WIDTH / 2;
static constexpr int WORDS_PER_FRAME =
    WORDS_PER_ROW * HEIGHT;

static constexpr int TOTAL_WORDS =
    WORDS_PER_FRAME * SLOTS;

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_frame_address dut;

    /*
     * bit 0 = lane 0 seen
     * bit 1 = lane 1 seen
     */
    std::vector<std::uint8_t> lane_mask(TOTAL_WORDS, 0);

    std::uint64_t pixel_count = 0;

    for (int slot = 0; slot < SLOTS; ++slot)
    {
        for (int y = 0; y < HEIGHT; ++y)
        {
            for (int x = 0; x < WIDTH; ++x)
            {
                dut.slot = slot;
                dut.x = x;
                dut.y = y;
                dut.eval();

                const int expected_address =
                    slot * WORDS_PER_FRAME +
                    y * WORDS_PER_ROW +
                    x / 2;

                const int expected_lane = x & 1;

                if (dut.word_address != expected_address)
                {
                    std::cerr
                        << "Address mismatch:"
                        << " slot=" << slot
                        << " x=" << x
                        << " y=" << y
                        << " expected=" << expected_address
                        << " got=" << dut.word_address
                        << "\n";
                    return 1;
                }

                if (dut.pixel_lane != expected_lane)
                {
                    std::cerr
                        << "Lane mismatch:"
                        << " slot=" << slot
                        << " x=" << x
                        << " y=" << y
                        << " expected=" << expected_lane
                        << " got="
                        << static_cast<int>(dut.pixel_lane)
                        << "\n";
                    return 1;
                }

                if (
                    expected_address < 0 ||
                    expected_address >= TOTAL_WORDS)
                {
                    std::cerr
                        << "Address outside framebuffer region: "
                        << expected_address << "\n";
                    return 1;
                }

                const std::uint8_t lane_bit =
                    static_cast<std::uint8_t>(
                        1u << expected_lane);

                if (lane_mask[expected_address] & lane_bit)
                {
                    std::cerr
                        << "Duplicate mapping:"
                        << " word=" << expected_address
                        << " lane=" << expected_lane
                        << "\n";
                    return 1;
                }

                lane_mask[expected_address] |= lane_bit;
                ++pixel_count;
            }
        }
    }

    for (int address = 0; address < TOTAL_WORDS; ++address)
    {
        if (lane_mask[address] != 0x03)
        {
            std::cerr
                << "Incomplete or overlapping SDRAM word at "
                << address
                << ": lane mask=0x"
                << std::hex
                << static_cast<int>(lane_mask[address])
                << std::dec
                << "\n";
            return 1;
        }
    }

    if (
        pixel_count !=
        static_cast<std::uint64_t>(WIDTH) *
            HEIGHT *
            SLOTS)
    {
        std::cerr
            << "Pixel count mismatch: "
            << pixel_count << "\n";
        return 1;
    }

    /*
     * Explicitly verify the final address of slot 3.
     */
    dut.slot = 3;
    dut.x = WIDTH - 1;
    dut.y = HEIGHT - 1;
    dut.eval();

    if (
        dut.word_address != TOTAL_WORDS - 1 ||
        dut.pixel_lane != 1)
    {
        std::cerr
            << "Final framebuffer address incorrect: word="
            << dut.word_address
            << " lane="
            << static_cast<int>(dut.pixel_lane)
            << "\n";
        return 1;
    }

    std::cout << "PASS: 4-slot framebuffer address mapping verified\n";
    std::cout
        << "PASS: "
        << pixel_count
        << " pixels mapped into "
        << TOTAL_WORDS
        << " 32-bit SDRAM words\n";
    std::cout << "PASS: no gaps or duplicate pixel lanes\n";
    std::cout
        << "PASS: framebuffer region size = "
        << TOTAL_WORDS * 4
        << " bytes\n";
    std::cout
        << "PASS: final address = word "
        << TOTAL_WORDS - 1
        << ", lane 1\n";

    return 0;
}
