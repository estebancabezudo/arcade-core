#include <algorithm>
#include <cstdint>
#include <deque>
#include <iostream>
#include <vector>

#include "Vvideo_capture_pipeline.h"
#include "verilated.h"

static constexpr int WIDTH = 320;
static constexpr int HEIGHT = 240;
static constexpr int PIXELS_PER_FRAME = WIDTH * HEIGHT;

static constexpr int SOURCE_FRAMES = 3;

/*
 * Independent clocks.
 */
static constexpr std::uint64_t SOURCE_HALF_PERIOD = 5;
static constexpr std::uint64_t MEMORY_HALF_PERIOD = 4;
static constexpr std::uint64_t HDMI_HALF_PERIOD = 7;

/*
 * One HDMI frame boundary roughly every source frame.
 *
 * This test is about integration, not refresh-rate conversion accuracy;
 * that policy was already tested separately.
 */
static constexpr int HDMI_FRAME_CLOCKS = 60000;

static constexpr int FRAME_START_GAP = 2;
static constexpr int FRAME_BLANK_CLOCKS = 200;

struct PendingWrite
{
    int due_cycle;
    std::uint32_t address;
    std::uint32_t data;
};

enum class SourceState
{
    FrameStart,
    StartGap,
    Pixels,
    Blank,
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

    Vvideo_capture_pipeline dut;

    dut.source_clk = 0;
    dut.memory_clk = 0;
    dut.hdmi_clk = 0;

    dut.source_reset = 1;
    dut.memory_reset = 1;
    dut.hdmi_reset = 1;

    dut.source_frame_start = 0;
    dut.source_frame_complete = 0;

    dut.pixel_valid = 0;
    dut.pixel_rgb565 = 0;
    dut.pixel_x = 0;
    dut.pixel_y = 0;

    dut.mem_write_ready = 0;
    dut.mem_write_done = 0;

    dut.hdmi_frame_start = 0;

    dut.eval();

    std::uint64_t now = 0;

    std::uint64_t next_source_toggle =
        SOURCE_HALF_PERIOD;

    std::uint64_t next_memory_toggle =
        MEMORY_HALF_PERIOD;

    std::uint64_t next_hdmi_toggle =
        HDMI_HALF_PERIOD;

    constexpr std::uint64_t RESET_RELEASE_TIME = 500;
    constexpr std::uint64_t MAX_TIME = 5000000;

    bool reset_released = false;

    SourceState source_state =
        SourceState::FrameStart;

    int source_frame = 0;
    int source_pixel = 0;
    int source_gap = 0;
    int source_blank = 0;

    int source_pixels_seen[SOURCE_FRAMES] = {
        0, 0, 0};

    bool saw_capture_failure = false;
    bool saw_memory_stall = false;

    int source_drop_count = 0;

    int memory_cycle = 0;
    int hdmi_cycle = 0;

    int accepted_memory_writes = 0;
    int completed_memory_writes = 0;

    int last_due_cycle = -1;

    std::deque<PendingWrite> pending_writes;

    std::vector<std::uint32_t> hdmi_frames_seen;

    bool saw_hdmi_frame_0 = false;
    bool saw_hdmi_frame_2 = false;

    /*
     * The deliberate memory stall occurs during frame 1.
     *
     * 4000 source pixels correspond to roughly 2000 packed writes,
     * well beyond the 512-entry command FIFO.
     */
    auto memory_should_stall = [&]()
    {
        return source_frame == 1 &&
               source_state == SourceState::Pixels &&
               source_pixel >= 1000 &&
               source_pixel < 5000;
    };

    while (now < MAX_TIME)
    {
        now = std::min(
            next_source_toggle,
            std::min(
                next_memory_toggle,
                next_hdmi_toggle));

        if (
            !reset_released &&
            now >= RESET_RELEASE_TIME)
        {
            dut.source_reset = 0;
            dut.memory_reset = 0;
            dut.hdmi_reset = 0;

            reset_released = true;

            dut.eval();
        }

        /*
         * SOURCE CLOCK DOMAIN
         */
        if (now == next_source_toggle)
        {
            next_source_toggle +=
                SOURCE_HALF_PERIOD;

            if (dut.source_clk == 0)
            {
                dut.source_frame_start = 0;
                dut.source_frame_complete = 0;

                dut.pixel_valid = 0;
                dut.pixel_rgb565 = 0;
                dut.pixel_x = 0;
                dut.pixel_y = 0;

                if (
                    reset_released &&
                    source_state != SourceState::Finished)
                {
                    switch (source_state)
                    {
                    case SourceState::FrameStart:
                        dut.source_frame_start = 1;
                        break;

                    case SourceState::StartGap:
                        break;

                    case SourceState::Pixels:
                    {
                        const int x =
                            source_pixel % WIDTH;

                        const int y =
                            source_pixel / WIDTH;

                        dut.pixel_valid = 1;
                        dut.pixel_x = x;
                        dut.pixel_y = y;

                        dut.pixel_rgb565 =
                            pixel_value(
                                source_frame,
                                x,
                                y);

                        if (
                            source_pixel ==
                            PIXELS_PER_FRAME - 1)
                        {
                            dut.source_frame_complete = 1;
                        }

                        break;
                    }

                    case SourceState::Blank:
                    case SourceState::Finished:
                        break;
                    }
                }

                dut.eval();

                dut.source_clk = 1;
                dut.eval();

                if (reset_released)
                {
                    if (dut.manager_error)
                    {
                        std::cerr
                            << "manager_error asserted\n";
                        return 1;
                    }

                    if (dut.source_frame_dropped)
                    {
                        ++source_drop_count;
                    }

                    if (
                        source_frame == 1 &&
                        dut.capture_failed)
                    {
                        saw_capture_failure = true;
                    }

                    switch (source_state)
                    {
                    case SourceState::FrameStart:
                        source_gap = 0;
                        source_state =
                            SourceState::StartGap;
                        break;

                    case SourceState::StartGap:
                        ++source_gap;

                        if (
                            source_gap >=
                            FRAME_START_GAP)
                        {
                            source_pixel = 0;
                            source_state =
                                SourceState::Pixels;
                        }
                        break;

                    case SourceState::Pixels:
                        ++source_pixels_seen[source_frame];

                        ++source_pixel;

                        if (
                            source_pixel >=
                            PIXELS_PER_FRAME)
                        {
                            source_blank = 0;
                            source_state =
                                SourceState::Blank;
                        }
                        break;

                    case SourceState::Blank:
                        ++source_blank;

                        if (
                            source_blank >=
                            FRAME_BLANK_CLOCKS)
                        {
                            ++source_frame;

                            if (
                                source_frame >=
                                SOURCE_FRAMES)
                            {
                                source_state =
                                    SourceState::Finished;
                            }
                            else
                            {
                                source_state =
                                    SourceState::FrameStart;
                            }
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
         * MEMORY CLOCK DOMAIN
         */
        if (now == next_memory_toggle)
        {
            next_memory_toggle +=
                MEMORY_HALF_PERIOD;

            if (dut.memory_clk == 0)
            {
                dut.mem_write_done = 0;

                if (reset_released)
                {
                    const bool force_stall =
                        memory_should_stall();

                    if (force_stall)
                    {
                        saw_memory_stall = true;
                    }

                    /*
                     * Outside the deliberate long stall, add a little
                     * irregular backpressure as well.
                     */
                    const bool short_stall =
                        (memory_cycle % 29) == 0;

                    dut.mem_write_ready =
                        !force_stall &&
                        !short_stall;

                    /*
                     * Complete at most one previously accepted write
                     * per memory clock.
                     */
                    if (
                        !pending_writes.empty() &&
                        pending_writes.front().due_cycle <=
                            memory_cycle)
                    {
                        dut.mem_write_done = 1;
                    }
                }
                else
                {
                    dut.mem_write_ready = 0;
                }

                dut.eval();

                const bool write_accepted =
                    dut.mem_write_valid &&
                    dut.mem_write_ready;

                const bool write_completed =
                    dut.mem_write_done;

                if (write_accepted)
                {
                    const std::uint32_t address =
                        dut.mem_write_address;

                    const std::uint32_t data =
                        dut.mem_write_data;

                    const int latency =
                        3 +
                        static_cast<int>(
                            address % 7);

                    int due_cycle =
                        memory_cycle + latency;

                    /*
                     * Responses remain ordered.
                     */
                    if (due_cycle <= last_due_cycle)
                    {
                        due_cycle =
                            last_due_cycle + 1;
                    }

                    last_due_cycle = due_cycle;

                    pending_writes.push_back({due_cycle,
                                              address,
                                              data});

                    ++accepted_memory_writes;
                }

                dut.memory_clk = 1;
                dut.eval();

                if (dut.memory_queue_error)
                {
                    std::cerr
                        << "memory_queue_error asserted\n";
                    return 1;
                }

                if (dut.result_bridge_error)
                {
                    std::cerr
                        << "result_bridge_error asserted\n";
                    return 1;
                }

                if (write_completed)
                {
                    if (pending_writes.empty())
                    {
                        std::cerr
                            << "Memory completed a write "
                            << "with no pending request\n";
                        return 1;
                    }

                    pending_writes.pop_front();
                    ++completed_memory_writes;
                }

                if (reset_released)
                {
                    ++memory_cycle;
                }
            }
            else
            {
                dut.memory_clk = 0;
                dut.eval();
            }
        }

        /*
         * HDMI CLOCK DOMAIN
         */
        if (now == next_hdmi_toggle)
        {
            next_hdmi_toggle +=
                HDMI_HALF_PERIOD;

            if (dut.hdmi_clk == 0)
            {
                dut.hdmi_frame_start = 0;

                if (
                    reset_released &&
                    (hdmi_cycle %
                     HDMI_FRAME_CLOCKS) == 0)
                {
                    dut.hdmi_frame_start = 1;
                }

                dut.eval();

                dut.hdmi_clk = 1;
                dut.eval();

                if (
                    reset_released &&
                    dut.hdmi_frame_start &&
                    dut.hdmi_frame_valid)
                {
                    const std::uint32_t frame_id =
                        dut.hdmi_frame_id;

                    hdmi_frames_seen.push_back(
                        frame_id);

                    if (frame_id == 0)
                    {
                        saw_hdmi_frame_0 = true;
                    }

                    if (frame_id == 1)
                    {
                        std::cerr
                            << "Aborted frame 1 became "
                            << "visible to HDMI\n";
                        return 1;
                    }

                    if (frame_id == 2)
                    {
                        saw_hdmi_frame_2 = true;
                    }
                }

                if (reset_released)
                {
                    ++hdmi_cycle;
                }
            }
            else
            {
                dut.hdmi_clk = 0;
                dut.eval();
            }
        }

        /*
         * Finish once all source frames have run, all physical writes have
         * drained, and HDMI has displayed frame 2.
         */
        if (
            reset_released &&
            source_state == SourceState::Finished &&
            pending_writes.empty() &&
            saw_hdmi_frame_2)
        {
            break;
        }
    }

    if (source_state != SourceState::Finished)
    {
        std::cerr
            << "Source raster did not finish all frames\n";
        return 1;
    }

    for (int frame = 0; frame < SOURCE_FRAMES; ++frame)
    {
        if (
            source_pixels_seen[frame] !=
            PIXELS_PER_FRAME)
        {
            std::cerr
                << "Frame "
                << frame
                << " raster was interrupted: expected "
                << PIXELS_PER_FRAME
                << " pixels, got "
                << source_pixels_seen[frame]
                << "\n";
            return 1;
        }
    }

    if (!saw_memory_stall)
    {
        std::cerr
            << "Deliberate memory stall was not exercised\n";
        return 1;
    }

    if (!saw_capture_failure)
    {
        std::cerr
            << "Frame 1 did not enter capture_failed\n";
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

    if (!saw_hdmi_frame_0)
    {
        std::cerr
            << "HDMI never displayed committed frame 0\n";
        return 1;
    }

    if (!saw_hdmi_frame_2)
    {
        std::cerr
            << "HDMI never displayed committed frame 2\n";
        return 1;
    }

    if (accepted_memory_writes == 0)
    {
        std::cerr << "No memory writes were accepted\n";
        return 1;
    }

    if (
        accepted_memory_writes !=
        completed_memory_writes)
    {
        std::cerr
            << "Memory writes remain incomplete: accepted "
            << accepted_memory_writes
            << ", completed "
            << completed_memory_writes
            << "\n";
        return 1;
    }

    std::cout
        << "PASS: integrated video capture pipeline verified\n";

    std::cout
        << "PASS: 3 source rasters delivered all "
        << PIXELS_PER_FRAME
        << " pixels without stalling\n";

    std::cout
        << "PASS: frame 1 aborted under memory backpressure\n";

    std::cout
        << "PASS: aborted frame 1 was never visible to HDMI\n";

    std::cout
        << "PASS: committed frames 0 and 2 became visible to HDMI\n";

    std::cout
        << "PASS: frame-manager capture drops = "
        << source_drop_count
        << "\n";

    std::cout
        << "PASS: "
        << completed_memory_writes
        << " physical memory writes completed\n";

    return 0;
}
