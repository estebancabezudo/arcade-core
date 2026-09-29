#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "Vvideo_framebuffer_pipeline.h"
#include "verilated.h"

static constexpr int WIDTH = 320;
static constexpr int HEIGHT = 240;
static constexpr int PIXELS_PER_FRAME = WIDTH * HEIGHT;
static constexpr int WORDS_PER_FRAME = PIXELS_PER_FRAME / 2;

static constexpr std::uint64_t SOURCE_HALF_PERIOD = 5;
static constexpr std::uint64_t VIDEO_HALF_PERIOD = 4;

struct PendingWrite
{
    int due_cycle;
    std::uint32_t address;
    std::uint32_t data;
};

struct PendingRead
{
    int due_cycle;
    std::uint32_t data;
};

enum class SourceState
{
    Frame0Start,
    Frame0Gap,
    Frame0Pixels,
    WaitForFrame0Hdmi,
    Frame1Start,
    Frame1Gap,
    Frame1Pixels,
    Finished
};

static std::uint16_t pixel_value(
    int frame,
    int x,
    int y)
{
    return static_cast<std::uint16_t>(
        ((frame * 0x4001u) ^
         (y * 0x1234u) ^
         (x * 0x01F3u) ^
         0xA55Au) &
        0xFFFFu);
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_framebuffer_pipeline dut;

    dut.source_clk = 0;
    dut.video_clk = 0;

    dut.source_reset = 1;
    dut.video_reset = 1;

    dut.source_frame_start = 0;
    dut.source_frame_complete = 0;

    dut.pixel_valid = 0;
    dut.pixel_rgb565 = 0;
    dut.pixel_x = 0;
    dut.pixel_y = 0;

    dut.hdmi_frame_start = 0;

    dut.line_fetch_start = 0;
    dut.line_fetch_y = 0;
    dut.line_release = 0;
    dut.line_read_x = 0;

    dut.memory_command_ready = 0;
    dut.memory_write_done = 0;
    dut.memory_read_response_valid = 0;
    dut.memory_read_response_data = 0;

    dut.eval();

    /*
     * Four framebuffer slots, 153,600 words total.
     */
    std::vector<std::uint32_t> memory(
        4 * WORDS_PER_FRAME,
        0);

    std::deque<PendingWrite> pending_writes;
    std::deque<PendingRead> pending_reads;

    std::uint64_t now = 0;

    std::uint64_t next_source_toggle =
        SOURCE_HALF_PERIOD;

    std::uint64_t next_video_toggle =
        VIDEO_HALF_PERIOD;

    constexpr std::uint64_t RESET_RELEASE_TIME = 500;
    constexpr std::uint64_t MAX_TIME = 5000000;

    bool reset_released = false;

    SourceState source_state =
        SourceState::Frame0Start;

    int source_gap = 0;
    int source_pixel = 0;

    std::array<int, 2> source_pixels_seen = {
        0,
        0};

    bool frame0_selected = false;
    bool frame1_selected = false;

    std::uint8_t frame0_slot = 0;

    int source_drop_count = 0;

    int accepted_writes = 0;
    int completed_writes = 0;

    int accepted_reads = 0;
    int completed_reads = 0;

    int last_write_due = -1;
    int last_read_due = -1;

    int video_cycle = 0;

    /*
     * Fetch three widely separated lines from frame 0 while frame 1
     * is being captured.
     */
    constexpr std::array<int, 3> LINES_TO_READ = {
        0,
        100,
        239};

    int line_request_index = 0;
    bool line_request_issued = false;

    bool saw_reads_while_frame1_capture = false;
    bool saw_writes_while_line_fetching = false;

    while (now < MAX_TIME)
    {
        now = std::min(
            next_source_toggle,
            next_video_toggle);

        if (
            !reset_released &&
            now >= RESET_RELEASE_TIME)
        {
            dut.source_reset = 0;
            dut.video_reset = 0;

            reset_released = true;
            dut.eval();
        }

        /*
         * SOURCE CLOCK DOMAIN
         */
        if (now == next_source_toggle)
        {
            next_source_toggle += SOURCE_HALF_PERIOD;

            if (dut.source_clk == 0)
            {
                dut.source_frame_start = 0;
                dut.source_frame_complete = 0;

                dut.pixel_valid = 0;
                dut.pixel_rgb565 = 0;
                dut.pixel_x = 0;
                dut.pixel_y = 0;

                if (reset_released)
                {
                    switch (source_state)
                    {
                    case SourceState::Frame0Start:
                        dut.source_frame_start = 1;
                        break;

                    case SourceState::Frame0Gap:
                        break;

                    case SourceState::Frame0Pixels:
                    {
                        const int x =
                            source_pixel % WIDTH;

                        const int y =
                            source_pixel / WIDTH;

                        dut.pixel_valid = 1;
                        dut.pixel_x = x;
                        dut.pixel_y = y;
                        dut.pixel_rgb565 =
                            pixel_value(0, x, y);

                        if (
                            source_pixel ==
                            PIXELS_PER_FRAME - 1)
                        {
                            dut.source_frame_complete = 1;
                        }

                        break;
                    }

                    case SourceState::WaitForFrame0Hdmi:
                        break;

                    case SourceState::Frame1Start:
                        dut.source_frame_start = 1;
                        break;

                    case SourceState::Frame1Gap:
                        break;

                    case SourceState::Frame1Pixels:
                    {
                        const int x =
                            source_pixel % WIDTH;

                        const int y =
                            source_pixel / WIDTH;

                        dut.pixel_valid = 1;
                        dut.pixel_x = x;
                        dut.pixel_y = y;
                        dut.pixel_rgb565 =
                            pixel_value(1, x, y);

                        if (
                            source_pixel ==
                            PIXELS_PER_FRAME - 1)
                        {
                            dut.source_frame_complete = 1;
                        }

                        break;
                    }

                    case SourceState::Finished:
                        break;
                    }
                }

                dut.eval();

                dut.source_clk = 1;
                dut.eval();

                if (reset_released)
                {
                    if (
                        dut.manager_error ||
                        dut.memory_queue_error ||
                        dut.result_bridge_error)
                    {
                        std::cerr
                            << "Pipeline diagnostic error asserted\n";
                        return 1;
                    }

                    if (dut.source_frame_dropped)
                    {
                        ++source_drop_count;
                    }

                    if (dut.capture_failed)
                    {
                        std::cerr
                            << "Unexpected capture_failed during "
                            << "normal framebuffer test\n";
                        return 1;
                    }

                    switch (source_state)
                    {
                    case SourceState::Frame0Start:
                        source_gap = 0;
                        source_state =
                            SourceState::Frame0Gap;
                        break;

                    case SourceState::Frame0Gap:
                        ++source_gap;

                        if (source_gap >= 2)
                        {
                            source_pixel = 0;
                            source_state =
                                SourceState::Frame0Pixels;
                        }

                        break;

                    case SourceState::Frame0Pixels:
                        ++source_pixels_seen[0];
                        ++source_pixel;

                        if (
                            source_pixel >=
                            PIXELS_PER_FRAME)
                        {
                            source_state =
                                SourceState::WaitForFrame0Hdmi;
                        }

                        break;

                    case SourceState::WaitForFrame0Hdmi:
                        if (frame0_selected)
                        {
                            source_state =
                                SourceState::Frame1Start;
                        }
                        break;

                    case SourceState::Frame1Start:
                        source_gap = 0;
                        source_state =
                            SourceState::Frame1Gap;
                        break;

                    case SourceState::Frame1Gap:
                        ++source_gap;

                        if (source_gap >= 2)
                        {
                            source_pixel = 0;
                            source_state =
                                SourceState::Frame1Pixels;
                        }

                        break;

                    case SourceState::Frame1Pixels:
                        ++source_pixels_seen[1];
                        ++source_pixel;

                        if (
                            source_pixel >=
                            PIXELS_PER_FRAME)
                        {
                            source_state =
                                SourceState::Finished;
                        }

                        break;

                    case SourceState::Finished:
                        break;
                    }
                }
            }
            else
            {
                dut.source_clk = 0;
                dut.eval();
            }
        }

        /*
         * VIDEO / MEMORY / HDMI CLOCK DOMAIN
         */
        if (now == next_video_toggle)
        {
            next_video_toggle += VIDEO_HALF_PERIOD;

            if (dut.video_clk == 0)
            {
                dut.hdmi_frame_start = 0;

                dut.line_fetch_start = 0;
                dut.line_release = 0;

                dut.memory_write_done = 0;
                dut.memory_read_response_valid = 0;
                dut.memory_read_response_data = 0;

                /*
                 * Physical memory controller is usually ready but
                 * occasionally applies short backpressure.
                 */
                dut.memory_command_ready =
                    (video_cycle % 23) != 0;

                /*
                 * Complete at most one write per cycle.
                 */
                if (
                    !pending_writes.empty() &&
                    pending_writes.front().due_cycle <=
                        video_cycle)
                {
                    dut.memory_write_done = 1;
                }

                /*
                 * Return at most one ordered read response per cycle.
                 */
                if (
                    !pending_reads.empty() &&
                    pending_reads.front().due_cycle <=
                        video_cycle)
                {
                    dut.memory_read_response_valid = 1;
                    dut.memory_read_response_data =
                        pending_reads.front().data;
                }

                /*
                 * Before frame 0 is selected, periodically give HDMI a
                 * frame boundary so it can adopt the first committed frame.
                 */
                if (
                    !frame0_selected &&
                    source_state ==
                        SourceState::WaitForFrame0Hdmi &&
                    (video_cycle % 256) == 0)
                {
                    dut.hdmi_frame_start = 1;
                }

                /*
                 * During capture of frame 1, request three lines from
                 * the already-committed frame 0.
                 */
                if (
                    frame0_selected &&
                    source_state ==
                        SourceState::Frame1Pixels &&
                    source_pixel > 5000 &&
                    line_request_index <
                        static_cast<int>(
                            LINES_TO_READ.size()) &&
                    !dut.line_busy &&
                    !dut.line_valid &&
                    !line_request_issued)
                {
                    dut.line_fetch_y =
                        LINES_TO_READ[line_request_index];

                    dut.line_fetch_start = 1;
                    line_request_issued = true;
                }

                /*
                 * Once a cached line is complete, verify all 320 pixels
                 * combinationally before releasing it.
                 */
                if (dut.line_valid)
                {
                    const int expected_y =
                        LINES_TO_READ[line_request_index];

                    if (
                        dut.line_slot != frame0_slot)
                    {
                        std::cerr
                            << "Fetched line came from wrong slot: "
                            << "expected "
                            << static_cast<int>(frame0_slot)
                            << ", got "
                            << static_cast<int>(dut.line_slot)
                            << "\n";
                        return 1;
                    }

                    if (dut.line_y != expected_y)
                    {
                        std::cerr
                            << "Fetched Y mismatch: expected "
                            << expected_y
                            << ", got "
                            << static_cast<int>(dut.line_y)
                            << "\n";
                        return 1;
                    }

                    for (int x = 0; x < WIDTH; ++x)
                    {
                        dut.line_read_x = x;
                        dut.eval();

                        const std::uint16_t expected =
                            pixel_value(
                                0,
                                x,
                                expected_y);

                        if (
                            dut.line_rgb565 != expected)
                        {
                            std::cerr
                                << "Fetched pixel mismatch at "
                                << x << ","
                                << expected_y
                                << ": expected 0x"
                                << std::hex
                                << expected
                                << ", got 0x"
                                << dut.line_rgb565
                                << std::dec
                                << "\n";
                            return 1;
                        }
                    }

                    dut.line_release = 1;
                }

                /*
                 * Once frame 1 has finished and all requested frame-0
                 * lines have been verified, periodically offer HDMI a
                 * new frame boundary until committed frame 1 is selected.
                 */
                if (
                    source_state ==
                        SourceState::Finished &&
                    line_request_index ==
                        static_cast<int>(
                            LINES_TO_READ.size()) &&
                    !frame1_selected &&
                    (video_cycle % 256) == 0)
                {
                    dut.hdmi_frame_start = 1;
                }

                dut.eval();

                const bool command_accepted =
                    dut.memory_command_valid &&
                    dut.memory_command_ready;

                bool accepted_write = false;
                bool accepted_read = false;

                if (command_accepted)
                {
                    if (dut.memory_command_write)
                    {
                        accepted_write = true;

                        const std::uint32_t address =
                            dut.memory_command_address;

                        if (address >= memory.size())
                        {
                            std::cerr
                                << "Write address outside framebuffer: "
                                << address << "\n";
                            return 1;
                        }

                        const int latency =
                            3 +
                            static_cast<int>(
                                address % 5);

                        int due =
                            video_cycle + latency;

                        if (due <= last_write_due)
                        {
                            due =
                                last_write_due + 1;
                        }

                        last_write_due = due;

                        pending_writes.push_back({due,
                                                  address,
                                                  dut.memory_command_write_data});

                        ++accepted_writes;
                    }
                    else
                    {
                        accepted_read = true;

                        const std::uint32_t address =
                            dut.memory_command_address;

                        if (address >= memory.size())
                        {
                            std::cerr
                                << "Read address outside framebuffer: "
                                << address << "\n";
                            return 1;
                        }

                        const int latency =
                            2 +
                            static_cast<int>(
                                address % 4);

                        int due =
                            video_cycle + latency;

                        if (due <= last_read_due)
                        {
                            due =
                                last_read_due + 1;
                        }

                        last_read_due = due;

                        pending_reads.push_back({due,
                                                 memory[address]});

                        ++accepted_reads;

                        if (
                            source_state ==
                            SourceState::Frame1Pixels)
                        {
                            saw_reads_while_frame1_capture =
                                true;
                        }
                    }
                }

                if (
                    accepted_write &&
                    (dut.line_busy ||
                     dut.line_valid))
                {
                    saw_writes_while_line_fetching = true;
                }

                dut.video_clk = 1;
                dut.eval();

                /*
                 * Apply physical completions after the active edge.
                 */
                if (dut.memory_write_done)
                {
                    if (pending_writes.empty())
                    {
                        std::cerr
                            << "Write completion with no pending write\n";
                        return 1;
                    }

                    const auto completed =
                        pending_writes.front();

                    pending_writes.pop_front();

                    memory[completed.address] =
                        completed.data;

                    ++completed_writes;
                }

                if (
                    dut.memory_read_response_valid)
                {
                    if (pending_reads.empty())
                    {
                        std::cerr
                            << "Read response with no pending read\n";
                        return 1;
                    }

                    pending_reads.pop_front();
                    ++completed_reads;
                }

                /*
                 * The start request was consumed on this video edge.
                 */
                if (dut.line_fetch_start)
                {
                    line_request_issued = false;
                }

                /*
                 * line_release was consumed on this edge.
                 */
                if (dut.line_release)
                {
                    ++line_request_index;
                }

                if (
                    dut.hdmi_frame_start &&
                    dut.hdmi_frame_valid)
                {
                    if (
                        !frame0_selected &&
                        dut.hdmi_frame_id == 0)
                    {
                        frame0_selected = true;
                        frame0_slot =
                            dut.hdmi_read_slot;
                    }

                    if (
                        frame0_selected &&
                        source_state ==
                            SourceState::Finished &&
                        dut.hdmi_frame_id == 1)
                    {
                        frame1_selected = true;
                    }
                }

                ++video_cycle;
            }
            else
            {
                dut.video_clk = 0;
                dut.eval();
            }
        }

        if (
            reset_released &&
            source_state == SourceState::Finished &&
            frame1_selected &&
            line_request_index ==
                static_cast<int>(
                    LINES_TO_READ.size()) &&
            pending_writes.empty() &&
            pending_reads.empty())
        {
            break;
        }
    }

    if (
        source_state !=
        SourceState::Finished)
    {
        std::cerr
            << "Source did not finish both frames\n";
        return 1;
    }

    if (
        source_pixels_seen[0] !=
            PIXELS_PER_FRAME ||
        source_pixels_seen[1] !=
            PIXELS_PER_FRAME)
    {
        std::cerr
            << "Source raster was interrupted: frame0="
            << source_pixels_seen[0]
            << ", frame1="
            << source_pixels_seen[1]
            << "\n";
        return 1;
    }

    if (source_drop_count != 0)
    {
        std::cerr
            << "Frame manager dropped "
            << source_drop_count
            << " frame(s)\n";
        return 1;
    }

    if (!frame0_selected)
    {
        std::cerr
            << "HDMI never selected frame 0\n";
        return 1;
    }

    if (!frame1_selected)
    {
        std::cerr
            << "HDMI never selected frame 1\n";
        return 1;
    }

    if (
        line_request_index !=
        static_cast<int>(
            LINES_TO_READ.size()))
    {
        std::cerr
            << "Not all requested framebuffer lines were verified\n";
        return 1;
    }

    if (accepted_reads != 3 * 160)
    {
        std::cerr
            << "Expected 480 framebuffer reads, got "
            << accepted_reads
            << "\n";
        return 1;
    }

    if (completed_reads != accepted_reads)
    {
        std::cerr
            << "Read completion mismatch: "
            << completed_reads
            << "/"
            << accepted_reads
            << "\n";
        return 1;
    }

    if (accepted_writes != 2 * WORDS_PER_FRAME)
    {
        std::cerr
            << "Expected "
            << 2 * WORDS_PER_FRAME
            << " framebuffer writes, got "
            << accepted_writes
            << "\n";
        return 1;
    }

    if (completed_writes != accepted_writes)
    {
        std::cerr
            << "Write completion mismatch: "
            << completed_writes
            << "/"
            << accepted_writes
            << "\n";
        return 1;
    }

    if (!saw_reads_while_frame1_capture)
    {
        std::cerr
            << "Framebuffer reads never overlapped frame-1 capture\n";
        return 1;
    }

    if (!saw_writes_while_line_fetching)
    {
        std::cerr
            << "Writes never resumed during line-fetch activity\n";
        return 1;
    }

    std::cout
        << "PASS: shared framebuffer pipeline verified\n";

    std::cout
        << "PASS: frames 0 and 1 captured without raster stalls\n";

    std::cout
        << "PASS: frame 0 read correctly while frame 1 was captured\n";

    std::cout
        << "PASS: lines 0, 100 and 239 reconstructed exactly\n";

    std::cout
        << "PASS: "
        << accepted_reads
        << " read commands completed\n";

    std::cout
        << "PASS: "
        << completed_writes
        << " framebuffer writes completed\n";

    std::cout
        << "PASS: reads and writes shared one memory port correctly\n";

    std::cout
        << "PASS: HDMI advanced from frame 0 to frame 1\n";

    return 0;
}
