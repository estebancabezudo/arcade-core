#include <cstdint>
#include <iostream>

#include "Vvideo_frame_packer.h"
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
static constexpr int WORDS_PER_FRAME = WORDS_PER_ROW * HEIGHT;

static void tick(Vvideo_frame_packer &dut)
{
    dut.clk = 1;
    dut.eval();

    dut.clk = 0;
    dut.eval();
}

static uint16_t pixel_value(int slot, int y, int x)
{
    return static_cast<uint16_t>(
        ((slot * 0x4001u) ^
         (y * 0x1234u) ^
         (x * 0x01F3u) ^
         0xA55Au) &
        0xFFFFu);
}

static int expected_address(int slot, int y, int x)
{
    return slot * WORDS_PER_FRAME +
           y * WORDS_PER_ROW +
           x / 2;
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_frame_packer dut;

    dut.clk = 0;
    dut.reset = 1;

    dut.in_valid = 0;
    dut.in_rgb565 = 0;
    dut.in_x = 0;
    dut.in_y = 0;
    dut.in_slot = 0;

    dut.eval();
    tick(dut);

    dut.reset = 0;
    dut.eval();

    int output_words = 0;

    /*
     * Exercise multiple rows in all four framebuffer slots.
     */
    for (int slot = 0; slot < SLOTS; ++slot)
    {
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < WIDTH; ++x)
            {
                dut.in_valid = 1;
                dut.in_slot = slot;
                dut.in_y = y;
                dut.in_x = x;
                dut.in_rgb565 = pixel_value(slot, y, x);

                tick(dut);

                if ((x & 1) == 0)
                {
                    if (dut.out_valid)
                    {
                        std::cerr
                            << "Unexpected output for even pixel:"
                            << " slot=" << slot
                            << " y=" << y
                            << " x=" << x
                            << "\n";
                        return 1;
                    }
                }
                else
                {
                    if (!dut.out_valid)
                    {
                        std::cerr
                            << "Missing output for odd pixel:"
                            << " slot=" << slot
                            << " y=" << y
                            << " x=" << x
                            << "\n";
                        return 1;
                    }

                    const uint16_t even_pixel =
                        pixel_value(slot, y, x - 1);

                    const uint16_t odd_pixel =
                        pixel_value(slot, y, x);

                    const uint32_t expected_data =
                        (static_cast<uint32_t>(odd_pixel) << 16) |
                        even_pixel;

                    const int expected_addr =
                        expected_address(slot, y, x);

                    if (dut.out_word_address != expected_addr)
                    {
                        std::cerr
                            << "Address mismatch:"
                            << " slot=" << slot
                            << " y=" << y
                            << " x=" << x
                            << " expected=" << expected_addr
                            << " got=" << dut.out_word_address
                            << "\n";
                        return 1;
                    }

                    if (dut.out_word_data != expected_data)
                    {
                        std::cerr
                            << "Data mismatch:"
                            << " slot=" << slot
                            << " y=" << y
                            << " pair=" << (x / 2)
                            << " expected=0x"
                            << std::hex << expected_data
                            << " got=0x"
                            << dut.out_word_data
                            << std::dec << "\n";
                        return 1;
                    }

                    ++output_words;
                }
            }
        }
    }

    dut.in_valid = 0;
    tick(dut);

    if (dut.out_valid)
    {
        std::cerr << "Output remained valid after input stopped\n";
        return 1;
    }

    constexpr int expected_words =
        SLOTS * 4 * (WIDTH / 2);

    if (output_words != expected_words)
    {
        std::cerr
            << "Word count mismatch: expected "
            << expected_words
            << ", got "
            << output_words
            << "\n";
        return 1;
    }

    /*
     * Explicit fourth-slot boundary check.
     */
    dut.in_valid = 1;
    dut.in_slot = 3;
    dut.in_y = HEIGHT - 1;

    dut.in_x = WIDTH - 2;
    dut.in_rgb565 = pixel_value(3, HEIGHT - 1, WIDTH - 2);
    tick(dut);

    if (dut.out_valid)
    {
        std::cerr << "Unexpected output for final even pixel\n";
        return 1;
    }

    dut.in_x = WIDTH - 1;
    dut.in_rgb565 = pixel_value(3, HEIGHT - 1, WIDTH - 1);
    tick(dut);

    if (!dut.out_valid)
    {
        std::cerr << "Missing final framebuffer word\n";
        return 1;
    }

    if (dut.out_word_address != SLOTS * WORDS_PER_FRAME - 1)
    {
        std::cerr
            << "Final word address mismatch: expected "
            << SLOTS * WORDS_PER_FRAME - 1
            << ", got "
            << dut.out_word_address
            << "\n";
        return 1;
    }

    std::cout << "PASS: RGB565 framebuffer packer verified\n";
    std::cout
        << "PASS: "
        << output_words
        << " 32-bit words generated correctly\n";
    std::cout << "PASS: all 4 framebuffer slots verified\n";
    std::cout << "PASS: row and slot boundaries remained isolated\n";
    std::cout
        << "PASS: final framebuffer word = "
        << SLOTS * WORDS_PER_FRAME - 1
        << "\n";

    return 0;
}
