#include <cstdint>
#include <iostream>

#include "Vvideo_capture_writer.h"
#include "verilated.h"

#ifndef TEST_SOURCE_WIDTH
#define TEST_SOURCE_WIDTH 320
#endif

#ifndef TEST_SOURCE_HEIGHT
#define TEST_SOURCE_HEIGHT 240
#endif

static constexpr int WIDTH = TEST_SOURCE_WIDTH;
static constexpr int HEIGHT = TEST_SOURCE_HEIGHT;
static constexpr int PIXELS_PER_FRAME = WIDTH * HEIGHT;
static constexpr int WORDS_PER_FRAME = PIXELS_PER_FRAME / 2;
static constexpr int WORDS_PER_ROW = WIDTH / 2;

static constexpr std::uint8_t COMMAND_WRITE = 0;
static constexpr std::uint8_t COMMAND_COMMIT = 1;
static constexpr std::uint8_t COMMAND_ABORT = 2;

static void tick(Vvideo_capture_writer &dut)
{
    dut.clk = 1;
    dut.eval();

    dut.clk = 0;
    dut.eval();
}

static uint16_t pixel_value(int frame, int x, int y)
{
    return static_cast<uint16_t>(
        ((frame * 0x4001u) ^
         (y * 0x1234u) ^
         (x * 0x01F3u) ^
         0xA55Au) &
        0xFFFFu);
}

static uint32_t packed_word(int frame, int x, int y)
{
    const uint16_t even_pixel =
        pixel_value(frame, x, y);

    const uint16_t odd_pixel =
        pixel_value(frame, x + 1, y);

    return (static_cast<uint32_t>(odd_pixel) << 16) |
           even_pixel;
}

static int word_address(int slot, int x, int y)
{
    return slot * WORDS_PER_FRAME +
           y * WORDS_PER_ROW +
           x / 2;
}

static bool check_protocol_error(
    Vvideo_capture_writer &dut)
{
    if (dut.protocol_error)
    {
        std::cerr << "Unexpected protocol_error\n";
        return false;
    }

    return true;
}

static bool run_successful_frame(
    Vvideo_capture_writer &dut)
{
    constexpr int FRAME = 0;
    constexpr int SLOT = 0;

    int write_count = 0;
    int commit_count = 0;
    int abort_count = 0;

    /*
     * Start frame one clock before the first pixel.
     */
    dut.frame_begin = 1;
    dut.frame_capture_enable = 1;
    dut.frame_slot = SLOT;
    dut.frame_id = FRAME;

    dut.command_ready = 1;

    tick(dut);

    dut.frame_begin = 0;
    dut.eval();

    if (!dut.capture_active)
    {
        std::cerr
            << "Successful frame did not enter capture_active\n";
        return false;
    }

    /*
     * Feed the entire source raster continuously:
     * exactly one source pixel per clock.
     */
    for (int y = 0; y < HEIGHT; ++y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            /*
             * Observe the command that will be accepted on this edge.
             */
            dut.eval();

            if (dut.command_valid && dut.command_ready)
            {
                if (dut.command_type == COMMAND_WRITE)
                {
                    const int word_index = write_count;

                    const int expected_y =
                        word_index / WORDS_PER_ROW;

                    const int expected_pair =
                        word_index % WORDS_PER_ROW;

                    const int expected_x =
                        expected_pair * 2;

                    const uint32_t expected_address =
                        word_address(
                            SLOT,
                            expected_x,
                            expected_y);

                    const uint32_t expected_data =
                        packed_word(
                            FRAME,
                            expected_x,
                            expected_y);

                    if (
                        dut.command_write_address !=
                        expected_address)
                    {
                        std::cerr
                            << "Successful frame address mismatch "
                            << "at word "
                            << word_index
                            << ": expected "
                            << expected_address
                            << ", got "
                            << dut.command_write_address
                            << "\n";
                        return false;
                    }

                    if (
                        dut.command_write_data !=
                        expected_data)
                    {
                        std::cerr
                            << "Successful frame data mismatch "
                            << "at word "
                            << word_index
                            << "\n";
                        return false;
                    }

                    ++write_count;
                }
                else if (
                    dut.command_type == COMMAND_COMMIT)
                {
                    ++commit_count;
                }
                else if (
                    dut.command_type == COMMAND_ABORT)
                {
                    ++abort_count;
                }
            }

            dut.pixel_valid = 1;
            dut.pixel_x = x;
            dut.pixel_y = y;
            dut.pixel_rgb565 =
                pixel_value(FRAME, x, y);

            dut.frame_end =
                (x == WIDTH - 1) &&
                (y == HEIGHT - 1);

            tick(dut);

            if (!check_protocol_error(dut))
                return false;
        }
    }

    dut.pixel_valid = 0;
    dut.frame_end = 0;
    dut.eval();

    /*
     * Drain the packer's final word and COMMIT marker.
     */
    for (int cycle = 0; cycle < 32; ++cycle)
    {
        dut.eval();

        if (dut.command_valid && dut.command_ready)
        {
            if (dut.command_type == COMMAND_WRITE)
            {
                const int word_index = write_count;

                if (word_index >= WORDS_PER_FRAME)
                {
                    std::cerr
                        << "Too many WRITE commands in "
                        << "successful frame\n";
                    return false;
                }

                const int expected_y =
                    word_index / WORDS_PER_ROW;

                const int expected_pair =
                    word_index % WORDS_PER_ROW;

                const int expected_x =
                    expected_pair * 2;

                const uint32_t expected_address =
                    word_address(
                        SLOT,
                        expected_x,
                        expected_y);

                const uint32_t expected_data =
                    packed_word(
                        FRAME,
                        expected_x,
                        expected_y);

                if (
                    dut.command_write_address !=
                        expected_address ||
                    dut.command_write_data !=
                        expected_data)
                {
                    std::cerr
                        << "Final successful WRITE mismatch "
                        << "at word "
                        << word_index
                        << "\n";
                    return false;
                }

                ++write_count;
            }
            else if (
                dut.command_type == COMMAND_COMMIT)
            {
                if (
                    dut.command_marker_slot != SLOT ||
                    dut.command_marker_frame_id != FRAME)
                {
                    std::cerr
                        << "Successful COMMIT metadata mismatch\n";
                    return false;
                }

                ++commit_count;
            }
            else if (
                dut.command_type == COMMAND_ABORT)
            {
                ++abort_count;
            }
        }

        tick(dut);

        if (!check_protocol_error(dut))
            return false;

        if (
            !dut.capture_active &&
            commit_count == 1)
        {
            break;
        }
    }

    if (write_count != WORDS_PER_FRAME)
    {
        std::cerr
            << "Successful frame expected "
            << WORDS_PER_FRAME
            << " WRITE commands, got "
            << write_count
            << "\n";
        return false;
    }

    if (commit_count != 1)
    {
        std::cerr
            << "Successful frame expected one COMMIT, got "
            << commit_count
            << "\n";
        return false;
    }

    if (abort_count != 0)
    {
        std::cerr
            << "Successful frame unexpectedly emitted ABORT\n";
        return false;
    }

    if (dut.capture_failed)
    {
        std::cerr
            << "Successful frame ended with capture_failed\n";
        return false;
    }

    std::cout
        << "PASS: successful frame produced "
        << write_count
        << " writes and one COMMIT\n";

    return true;
}

static bool run_aborted_frame(
    Vvideo_capture_writer &dut)
{
    constexpr int FRAME = 1;
    constexpr int SLOT = 1;

    int raster_pixels = 0;
    int write_count = 0;
    int commit_count = 0;
    int abort_count = 0;

    bool stall_started = false;
    int stall_cycles = 0;

    /*
     * Start frame 1.
     */
    dut.frame_begin = 1;
    dut.frame_capture_enable = 1;
    dut.frame_slot = SLOT;
    dut.frame_id = FRAME;

    dut.command_ready = 1;

    tick(dut);

    dut.frame_begin = 0;
    dut.eval();

    if (!dut.capture_active)
    {
        std::cerr
            << "Abort test frame did not enter capture_active\n";
        return false;
    }

    /*
     * Feed the WHOLE raster regardless of capture_failed.
     *
     * After a few successful writes, hold command_ready low long enough
     * for another packed word to arrive while the skid buffer is occupied.
     */
    for (int y = 0; y < HEIGHT; ++y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            if (
                !stall_started &&
                write_count >= 20)
            {
                stall_started = true;
                stall_cycles = 12;
            }

            if (stall_cycles > 0)
            {
                dut.command_ready = 0;
                --stall_cycles;
            }
            else
            {
                dut.command_ready = 1;
            }

            dut.eval();

            if (dut.command_valid && dut.command_ready)
            {
                if (dut.command_type == COMMAND_WRITE)
                {
                    ++write_count;
                }
                else if (
                    dut.command_type == COMMAND_COMMIT)
                {
                    ++commit_count;
                }
                else if (
                    dut.command_type == COMMAND_ABORT)
                {
                    ++abort_count;
                }
            }

            /*
             * The raster NEVER waits for command_ready.
             */
            dut.pixel_valid = 1;
            dut.pixel_x = x;
            dut.pixel_y = y;
            dut.pixel_rgb565 =
                pixel_value(FRAME, x, y);

            dut.frame_end =
                (x == WIDTH - 1) &&
                (y == HEIGHT - 1);

            tick(dut);

            ++raster_pixels;

            if (!check_protocol_error(dut))
                return false;
        }
    }

    dut.pixel_valid = 0;
    dut.frame_end = 0;
    dut.command_ready = 1;
    dut.eval();

    /*
     * Drain the retained partial WRITE and the ABORT marker.
     */
    for (int cycle = 0; cycle < 64; ++cycle)
    {
        dut.eval();

        if (dut.command_valid && dut.command_ready)
        {
            if (dut.command_type == COMMAND_WRITE)
            {
                ++write_count;
            }
            else if (
                dut.command_type == COMMAND_COMMIT)
            {
                ++commit_count;
            }
            else if (
                dut.command_type == COMMAND_ABORT)
            {
                if (
                    dut.command_marker_slot != SLOT ||
                    dut.command_marker_frame_id != FRAME)
                {
                    std::cerr
                        << "ABORT metadata mismatch\n";
                    return false;
                }

                ++abort_count;
            }
        }

        tick(dut);

        if (!check_protocol_error(dut))
            return false;

        if (
            !dut.capture_active &&
            abort_count == 1)
        {
            break;
        }
    }

    if (!stall_started)
    {
        std::cerr
            << "Abort test never started FIFO stall\n";
        return false;
    }

    if (raster_pixels != PIXELS_PER_FRAME)
    {
        std::cerr
            << "Raster was interrupted: expected "
            << PIXELS_PER_FRAME
            << " pixels, got "
            << raster_pixels
            << "\n";
        return false;
    }

    if (!dut.capture_failed)
    {
        std::cerr
            << "FIFO stall did not cause capture_failed\n";
        return false;
    }

    if (write_count >= WORDS_PER_FRAME)
    {
        std::cerr
            << "Abort test unexpectedly captured a complete frame\n";
        return false;
    }

    if (commit_count != 0)
    {
        std::cerr
            << "Aborted frame emitted COMMIT\n";
        return false;
    }

    if (abort_count != 1)
    {
        std::cerr
            << "Expected exactly one ABORT, got "
            << abort_count
            << "\n";
        return false;
    }

    std::cout
        << "PASS: FIFO stall aborted capture after "
        << write_count
        << " queued writes\n";

    std::cout
        << "PASS: raster continued for all "
        << raster_pixels
        << " pixels without waiting\n";

    std::cout
        << "PASS: aborted frame emitted ABORT and no COMMIT\n";

    return true;
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_capture_writer dut;

    dut.clk = 0;
    dut.reset = 1;

    dut.frame_begin = 0;
    dut.frame_capture_enable = 0;
    dut.frame_slot = 0;
    dut.frame_id = 0;

    dut.pixel_valid = 0;
    dut.pixel_rgb565 = 0;
    dut.pixel_x = 0;
    dut.pixel_y = 0;

    dut.frame_end = 0;

    dut.command_ready = 0;

    dut.eval();
    tick(dut);

    dut.reset = 0;
    dut.eval();

    if (!run_successful_frame(dut))
        return 1;

    /*
     * One idle clock between frames.
     */
    dut.frame_begin = 0;
    dut.pixel_valid = 0;
    dut.frame_end = 0;
    dut.command_ready = 1;

    tick(dut);

    if (!run_aborted_frame(dut))
        return 1;

    std::cout
        << "PASS: video capture writer verified\n";

    std::cout
        << "PASS: framebuffer backpressure never stalled source raster\n";

    return 0;
}
