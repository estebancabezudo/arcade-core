#include <algorithm>
#include <cstdint>
#include <deque>
#include <iostream>
#include <unordered_set>

#include "Vvideo_frame_manager.h"
#include "verilated.h"

static constexpr std::uint64_t SOURCE_HALF_PERIOD = 5;
static constexpr std::uint64_t HDMI_HALF_PERIOD = 7;

static constexpr int SOURCE_FRAME_CLOCKS = 693;
static constexpr int SOURCE_COMPLETE_PHASE = 650;
static constexpr int HDMI_FRAME_CLOCKS = 500;

static constexpr int HDMI_FRAMES_TO_SIMULATE = 300;

struct PendingCommit
{
    std::uint64_t due_source_clock;
    std::uint32_t frame_id;
    std::uint8_t slot;
};

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_frame_manager dut;

    dut.source_clk = 0;
    dut.hdmi_clk = 0;

    dut.source_reset = 1;
    dut.hdmi_reset = 1;

    dut.source_frame_start = 0;
    dut.source_frame_complete = 0;

    dut.source_commit_valid = 0;
    dut.source_commit_slot = 0;
    dut.source_commit_frame_id = 0;

    dut.source_abort_valid = 0;
    dut.source_abort_slot = 0;
    dut.source_abort_frame_id = 0;

    dut.hdmi_frame_start = 0;

    dut.eval();

    std::uint64_t now = 0;
    std::uint64_t next_source_toggle = SOURCE_HALF_PERIOD;
    std::uint64_t next_hdmi_toggle = HDMI_HALF_PERIOD;

    constexpr std::uint64_t RESET_RELEASE_TIME = 1000;

    bool reset_released = false;

    std::uint64_t source_clock_count = 0;
    std::uint64_t hdmi_clock_count = 0;

    int source_frames_started = 0;
    int source_frames_completed = 0;
    int source_frames_committed = 0;
    int source_frames_dropped = 0;

    int hdmi_frame_boundaries = 0;
    int hdmi_valid_frames = 0;
    int hdmi_skipped_source_frames = 0;

    bool have_previous_hdmi_id = false;
    std::uint32_t previous_hdmi_id = 0;

    std::deque<PendingCommit> pending_commits;

    std::unordered_set<std::uint32_t> committed_frames;
    std::unordered_set<std::uint32_t> completed_not_committed;

    std::uint32_t active_frame_id = 0;
    std::uint8_t active_slot = 0;
    bool active_capture = false;

    bool saw_completed_uncommitted_window = false;

    while (hdmi_frame_boundaries < HDMI_FRAMES_TO_SIMULATE)
    {
        now = std::min(
            next_source_toggle,
            next_hdmi_toggle);

        if (
            !reset_released &&
            now >= RESET_RELEASE_TIME)
        {
            dut.source_reset = 0;
            dut.hdmi_reset = 0;

            source_clock_count = 0;
            hdmi_clock_count = 0;

            reset_released = true;
            dut.eval();
        }

        /*
         * Source clock domain.
         */
        if (now == next_source_toggle)
        {
            next_source_toggle += SOURCE_HALF_PERIOD;

            if (dut.source_clk == 0)
            {
                dut.source_frame_start = 0;
                dut.source_frame_complete = 0;

                dut.source_commit_valid = 0;
                dut.source_commit_slot = 0;
                dut.source_commit_frame_id = 0;

                /*
                 * This test exercises only successful commits.
                 */
                dut.source_abort_valid = 0;
                dut.source_abort_slot = 0;
                dut.source_abort_frame_id = 0;

                if (reset_released)
                {
                    const int phase =
                        static_cast<int>(
                            source_clock_count %
                            SOURCE_FRAME_CLOCKS);

                    if (phase == 0)
                        dut.source_frame_start = 1;

                    if (phase == SOURCE_COMPLETE_PHASE)
                        dut.source_frame_complete = 1;

                    /*
                     * Simulate writes draining through FIFO/SDRAM after
                     * raster completion.
                     */
                    if (
                        !pending_commits.empty() &&
                        pending_commits.front().due_source_clock <=
                            source_clock_count)
                    {
                        const auto commit =
                            pending_commits.front();

                        dut.source_commit_valid = 1;
                        dut.source_commit_slot =
                            commit.slot;
                        dut.source_commit_frame_id =
                            commit.frame_id;
                    }
                }

                dut.eval();

                dut.source_clk = 1;
                dut.eval();

                if (reset_released)
                {
                    if (dut.source_commit_error)
                    {
                        std::cerr
                            << "Unexpected source_commit_error\n";
                        return 1;
                    }

                    if (dut.source_abort_error)
                    {
                        std::cerr
                            << "Unexpected source_abort_error\n";
                        return 1;
                    }

                    if (dut.source_commit_valid)
                    {
                        const auto commit =
                            pending_commits.front();

                        committed_frames.insert(
                            commit.frame_id);

                        completed_not_committed.erase(
                            commit.frame_id);

                        pending_commits.pop_front();

                        ++source_frames_committed;
                    }

                    if (dut.source_frame_start)
                    {
                        ++source_frames_started;

                        active_frame_id =
                            static_cast<std::uint32_t>(
                                source_frames_started - 1);

                        active_capture =
                            dut.source_capture_enable;

                        active_slot =
                            static_cast<std::uint8_t>(
                                dut.source_write_slot);

                        if (dut.source_frame_dropped)
                        {
                            ++source_frames_dropped;
                            active_capture = false;
                        }
                    }

                    if (dut.source_frame_complete)
                    {
                        ++source_frames_completed;

                        if (active_capture)
                        {
                            /*
                             * Commit 120 source clocks after raster
                             * completion.
                             */
                            pending_commits.push_back({source_clock_count + 120,
                                                       active_frame_id,
                                                       active_slot});

                            completed_not_committed.insert(
                                active_frame_id);
                        }

                        active_capture = false;
                    }

                    ++source_clock_count;
                }
            }
            else
            {
                dut.source_clk = 0;
                dut.eval();
            }
        }

        /*
         * HDMI clock domain.
         */
        if (now == next_hdmi_toggle)
        {
            next_hdmi_toggle += HDMI_HALF_PERIOD;

            if (dut.hdmi_clk == 0)
            {
                dut.hdmi_frame_start = 0;

                if (
                    reset_released &&
                    (hdmi_clock_count %
                     HDMI_FRAME_CLOCKS) == 0)
                {
                    dut.hdmi_frame_start = 1;
                }

                dut.eval();

                dut.hdmi_clk = 1;
                dut.eval();

                if (reset_released)
                {
                    if (dut.hdmi_frame_start)
                    {
                        ++hdmi_frame_boundaries;

                        if (dut.hdmi_frame_valid)
                        {
                            ++hdmi_valid_frames;

                            const std::uint32_t current_id =
                                dut.hdmi_frame_id;

                            /*
                             * HDMI must never see a framebuffer before
                             * the memory path has committed it.
                             */
                            if (
                                committed_frames.find(current_id) ==
                                committed_frames.end())
                            {
                                std::cerr
                                    << "HDMI exposed uncommitted frame "
                                    << current_id
                                    << "\n";
                                return 1;
                            }

                            if (
                                completed_not_committed.find(
                                    current_id) !=
                                completed_not_committed.end())
                            {
                                std::cerr
                                    << "HDMI exposed raster-complete "
                                    << "but uncommitted frame "
                                    << current_id
                                    << "\n";
                                return 1;
                            }

                            if (have_previous_hdmi_id)
                            {
                                if (
                                    current_id <
                                    previous_hdmi_id)
                                {
                                    std::cerr
                                        << "HDMI frame IDs moved backwards: "
                                        << previous_hdmi_id
                                        << " -> "
                                        << current_id
                                        << "\n";
                                    return 1;
                                }

                                if (
                                    current_id >
                                    previous_hdmi_id + 1)
                                {
                                    hdmi_skipped_source_frames +=
                                        static_cast<int>(
                                            current_id -
                                            previous_hdmi_id -
                                            1);
                                }
                            }

                            previous_hdmi_id = current_id;
                            have_previous_hdmi_id = true;
                        }

                        if (!completed_not_committed.empty())
                        {
                            saw_completed_uncommitted_window =
                                true;
                        }
                    }

                    ++hdmi_clock_count;
                }
            }
            else
            {
                dut.hdmi_clk = 0;
                dut.eval();
            }
        }
    }

    if (!saw_completed_uncommitted_window)
    {
        std::cerr
            << "Test never exercised "
            << "raster-complete/uncommitted state\n";
        return 1;
    }

    if (source_frames_committed == 0)
    {
        std::cerr << "No frames were committed\n";
        return 1;
    }

    if (hdmi_valid_frames == 0)
    {
        std::cerr
            << "HDMI never received a committed frame\n";
        return 1;
    }

    if (source_frames_dropped != 0)
    {
        std::cerr
            << "Source capture drops detected: "
            << source_frames_dropped
            << "\n";
        return 1;
    }

    std::cout
        << "PASS: committed-frame publication verified\n";

    std::cout
        << "PASS: source started "
        << source_frames_started
        << ", completed "
        << source_frames_completed
        << ", committed "
        << source_frames_committed
        << " frames\n";

    std::cout
        << "PASS: source capture drops = "
        << source_frames_dropped
        << "\n";

    std::cout
        << "PASS: HDMI valid frames = "
        << hdmi_valid_frames
        << "\n";

    std::cout
        << "PASS: visually skipped source frames = "
        << hdmi_skipped_source_frames
        << "\n";

    std::cout
        << "PASS: HDMI never exposed an uncommitted framebuffer\n";

    return 0;
}
