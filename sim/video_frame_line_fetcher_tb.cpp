#include <cstdint>
#include <deque>
#include <iostream>

#include "Vvideo_frame_line_fetcher.h"
#include "verilated.h"

#ifndef TEST_SOURCE_WIDTH
#define TEST_SOURCE_WIDTH 320
#endif

#ifndef TEST_SOURCE_HEIGHT
#define TEST_SOURCE_HEIGHT 240
#endif

static constexpr int WIDTH = TEST_SOURCE_WIDTH;
static constexpr int HEIGHT = TEST_SOURCE_HEIGHT;
static constexpr int WORDS_PER_ROW = WIDTH / 2;
static constexpr int WORDS_PER_FRAME = WORDS_PER_ROW * HEIGHT;

struct PendingResponse
{
    int due_cycle;
    uint32_t data;
};

static void tick(Vvideo_frame_line_fetcher &dut)
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

static uint32_t memory_word(uint32_t address)
{
    const int slot = address / WORDS_PER_FRAME;
    const int frame_offset = address % WORDS_PER_FRAME;

    const int y = frame_offset / WORDS_PER_ROW;
    const int word_x = frame_offset % WORDS_PER_ROW;

    const int even_x = word_x * 2;
    const int odd_x = even_x + 1;

    const uint16_t even_pixel =
        pixel_value(slot, y, even_x);

    const uint16_t odd_pixel =
        pixel_value(slot, y, odd_x);

    return (static_cast<uint32_t>(odd_pixel) << 16) |
           even_pixel;
}

static bool verify_line(
    Vvideo_frame_line_fetcher &dut,
    int slot,
    int y)
{
    for (int x = 0; x < WIDTH; ++x)
    {
        dut.read_x = x;
        dut.eval();

        const uint16_t expected =
            pixel_value(slot, y, x);

        if (dut.read_rgb565 != expected)
        {
            std::cerr
                << "Cached pixel mismatch:"
                << " slot=" << slot
                << " y=" << y
                << " x=" << x
                << " expected=0x"
                << std::hex << expected
                << " got=0x"
                << dut.read_rgb565
                << std::dec << "\n";
            return false;
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_frame_line_fetcher dut;

    dut.clk = 0;
    dut.reset = 1;

    dut.start = 0;
    dut.request_slot = 0;
    dut.request_y = 0;
    dut.release_line = 0;

    dut.read_x = 0;

    dut.mem_read_ready = 0;
    dut.mem_response_valid = 0;
    dut.mem_response_data = 0;

    dut.eval();
    tick(dut);

    dut.reset = 0;
    dut.eval();

    constexpr int TEST_SLOT = 3;
    constexpr int TEST_Y = HEIGHT - 1;

    constexpr uint32_t EXPECTED_FIRST_ADDRESS =
        4 * WORDS_PER_FRAME - WORDS_PER_ROW;
    constexpr uint32_t EXPECTED_LAST_ADDRESS =
        4 * WORDS_PER_FRAME - 1;

    dut.request_slot = TEST_SLOT;
    dut.request_y = TEST_Y;
    dut.start = 1;

    tick(dut);

    dut.start = 0;
    dut.eval();

    if (!dut.busy)
    {
        std::cerr << "Fetcher did not enter busy state\n";
        return 1;
    }

    std::deque<PendingResponse> pending;

    int accepted_requests = 0;
    int delivered_responses = 0;
    int last_due_cycle = -1;

    uint32_t first_address = 0;
    uint32_t last_address = 0;

    constexpr int MAX_CYCLES = 5000;

    for (
        int cycle = 0;
        cycle < MAX_CYCLES && !dut.line_valid;
        ++cycle)
    {
        /*
         * Deliberately irregular memory backpressure.
         */
        dut.mem_read_ready =
            (cycle % 5 != 0) &&
            (cycle % 11 != 0);

        /*
         * Deliver at most one response per cycle.
         */
        if (
            !pending.empty() &&
            pending.front().due_cycle <= cycle)
        {
            dut.mem_response_valid = 1;
            dut.mem_response_data = pending.front().data;
        }
        else
        {
            dut.mem_response_valid = 0;
            dut.mem_response_data = 0;
        }

        dut.eval();

        const bool request_accepted =
            dut.mem_read_valid &&
            dut.mem_read_ready;

        const bool response_delivered =
            dut.mem_response_valid;

        if (request_accepted)
        {
            const uint32_t address =
                dut.mem_read_address;

            if (accepted_requests == 0)
                first_address = address;

            last_address = address;

            /*
             * Variable latency from 2 to 8 cycles.
             *
             * Responses remain ordered even when a later request would
             * otherwise have a shorter latency.
             */
            const int latency =
                2 + static_cast<int>(address % 7);

            int due_cycle = cycle + latency;

            if (due_cycle <= last_due_cycle)
                due_cycle = last_due_cycle + 1;

            last_due_cycle = due_cycle;

            pending.push_back({due_cycle,
                               memory_word(address)});

            ++accepted_requests;
        }

        tick(dut);

        if (response_delivered)
        {
            pending.pop_front();
            ++delivered_responses;
        }
    }

    dut.mem_read_ready = 0;
    dut.mem_response_valid = 0;
    dut.eval();

    if (!dut.line_valid)
    {
        std::cerr << "Fetcher did not complete line load\n";
        return 1;
    }

    if (dut.busy)
    {
        std::cerr
            << "Fetcher remained busy after line completion\n";
        return 1;
    }

    if (accepted_requests != WORDS_PER_ROW)
    {
        std::cerr
            << "Expected " << WORDS_PER_ROW
            << " accepted read requests, got "
            << accepted_requests << "\n";
        return 1;
    }

    if (delivered_responses != WORDS_PER_ROW)
    {
        std::cerr
            << "Expected " << WORDS_PER_ROW
            << " delivered responses, got "
            << delivered_responses << "\n";
        return 1;
    }

    if (first_address != EXPECTED_FIRST_ADDRESS)
    {
        std::cerr
            << "First address mismatch: expected "
            << EXPECTED_FIRST_ADDRESS
            << ", got "
            << first_address
            << "\n";
        return 1;
    }

    if (last_address != EXPECTED_LAST_ADDRESS)
    {
        std::cerr
            << "Last address mismatch: expected "
            << EXPECTED_LAST_ADDRESS
            << ", got "
            << last_address
            << "\n";
        return 1;
    }

    if (dut.line_slot != TEST_SLOT)
    {
        std::cerr
            << "line_slot mismatch: expected "
            << TEST_SLOT
            << ", got "
            << static_cast<int>(dut.line_slot)
            << "\n";
        return 1;
    }

    if (dut.line_y != TEST_Y)
    {
        std::cerr
            << "line_y mismatch: expected "
            << TEST_Y
            << ", got "
            << static_cast<int>(dut.line_y)
            << "\n";
        return 1;
    }

    if (!verify_line(dut, TEST_SLOT, TEST_Y))
        return 1;

    /*
     * The line must remain readable until explicitly released.
     */
    for (int pass = 0; pass < 3; ++pass)
    {
        if (!dut.line_valid)
        {
            std::cerr
                << "line_valid disappeared before release\n";
            return 1;
        }

        if (!verify_line(dut, TEST_SLOT, TEST_Y))
            return 1;
    }

    dut.release_line = 1;
    tick(dut);

    dut.release_line = 0;
    dut.eval();

    if (dut.line_valid)
    {
        std::cerr << "Line remained valid after release\n";
        return 1;
    }

    std::cout << "PASS: framebuffer line fetcher verified\n";
    std::cout
        << "PASS: slot 3 / line " << TEST_Y
        << " reconstructed correctly\n";
    std::cout
        << "PASS: SDRAM addresses " << EXPECTED_FIRST_ADDRESS
        << ".." << EXPECTED_LAST_ADDRESS << " verified\n";
    std::cout
        << "PASS: " << WORDS_PER_ROW
        << " SDRAM words reconstructed " << WIDTH
        << " RGB565 pixels\n";
    std::cout << "PASS: memory backpressure exercised\n";
    std::cout << "PASS: variable ordered response latency exercised\n";
    std::cout << "PASS: cached line remained stable for repeated reads\n";

    return 0;
}
