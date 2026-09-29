#include <algorithm>
#include <cstdint>
#include <deque>
#include <iostream>

#include "Vvideo_memory_result_bridge.h"
#include "verilated.h"

static constexpr std::uint64_t MEMORY_HALF_PERIOD = 5;
static constexpr std::uint64_t SOURCE_HALF_PERIOD = 7;

struct Result
{
    bool abort;
    std::uint8_t slot;
    std::uint32_t frame_id;
};

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_memory_result_bridge dut;

    dut.memory_clk = 0;
    dut.source_clk = 0;

    dut.memory_reset = 1;
    dut.source_reset = 1;

    dut.memory_commit_valid = 0;
    dut.memory_commit_slot = 0;
    dut.memory_commit_frame_id = 0;

    dut.memory_abort_valid = 0;
    dut.memory_abort_slot = 0;
    dut.memory_abort_frame_id = 0;

    dut.eval();

    const std::deque<Result> source_results = {
        {false, 0, 100},
        {true, 1, 101},
        {false, 2, 102},
        {false, 3, 103},
        {true, 0, 104},
        {true, 2, 105},
        {false, 1, 106},
        {false, 3, 107},
        {true, 1, 108},
        {false, 0, 109}};

    std::deque<Result> pending_input = source_results;
    std::deque<Result> expected_output = source_results;

    std::uint64_t now = 0;
    std::uint64_t next_memory_toggle = MEMORY_HALF_PERIOD;
    std::uint64_t next_source_toggle = SOURCE_HALF_PERIOD;

    constexpr std::uint64_t RESET_RELEASE_TIME = 200;
    constexpr std::uint64_t MAX_TIME = 200000;

    bool reset_released = false;

    int results_sent = 0;
    int results_received = 0;

    while (
        now < MAX_TIME &&
        results_received < static_cast<int>(source_results.size()))
    {
        now = std::min(
            next_memory_toggle,
            next_source_toggle);

        if (!reset_released && now >= RESET_RELEASE_TIME)
        {
            dut.memory_reset = 0;
            dut.source_reset = 0;

            reset_released = true;
            dut.eval();
        }

        /*
         * MEMORY CLOCK DOMAIN
         */
        if (now == next_memory_toggle)
        {
            next_memory_toggle += MEMORY_HALF_PERIOD;

            if (dut.memory_clk == 0)
            {
                dut.memory_commit_valid = 0;
                dut.memory_abort_valid = 0;

                if (
                    reset_released &&
                    !pending_input.empty() &&
                    dut.memory_result_ready)
                {
                    const auto &result =
                        pending_input.front();

                    if (result.abort)
                    {
                        dut.memory_abort_valid = 1;
                        dut.memory_abort_slot =
                            result.slot;
                        dut.memory_abort_frame_id =
                            result.frame_id;
                    }
                    else
                    {
                        dut.memory_commit_valid = 1;
                        dut.memory_commit_slot =
                            result.slot;
                        dut.memory_commit_frame_id =
                            result.frame_id;
                    }
                }

                dut.eval();

                const bool accepted =
                    dut.memory_result_ready &&
                    (dut.memory_commit_valid ||
                     dut.memory_abort_valid);

                dut.memory_clk = 1;
                dut.eval();

                if (dut.memory_protocol_error)
                {
                    std::cerr
                        << "Unexpected memory_protocol_error\n";
                    return 1;
                }

                if (accepted)
                {
                    pending_input.pop_front();
                    ++results_sent;
                }
            }
            else
            {
                dut.memory_clk = 0;
                dut.eval();
            }
        }

        /*
         * SOURCE CLOCK DOMAIN
         */
        if (now == next_source_toggle)
        {
            next_source_toggle += SOURCE_HALF_PERIOD;

            if (dut.source_clk == 0)
            {
                dut.eval();

                const bool commit =
                    dut.source_commit_valid;

                const bool abort =
                    dut.source_abort_valid;

                if (commit && abort)
                {
                    std::cerr
                        << "COMMIT and ABORT visible simultaneously\n";
                    return 1;
                }

                if (commit || abort)
                {
                    if (expected_output.empty())
                    {
                        std::cerr
                            << "Unexpected result in source domain\n";
                        return 1;
                    }

                    const auto expected =
                        expected_output.front();

                    if (abort != expected.abort)
                    {
                        std::cerr
                            << "Result type mismatch for frame "
                            << expected.frame_id << "\n";
                        return 1;
                    }

                    const std::uint8_t actual_slot =
                        commit
                            ? dut.source_commit_slot
                            : dut.source_abort_slot;

                    const std::uint32_t actual_frame_id =
                        commit
                            ? dut.source_commit_frame_id
                            : dut.source_abort_frame_id;

                    if (actual_slot != expected.slot)
                    {
                        std::cerr
                            << "Slot mismatch: expected "
                            << static_cast<int>(expected.slot)
                            << ", got "
                            << static_cast<int>(actual_slot)
                            << "\n";
                        return 1;
                    }

                    if (actual_frame_id != expected.frame_id)
                    {
                        std::cerr
                            << "Frame ID mismatch: expected "
                            << expected.frame_id
                            << ", got "
                            << actual_frame_id
                            << "\n";
                        return 1;
                    }

                    expected_output.pop_front();
                    ++results_received;
                }

                dut.source_clk = 1;
                dut.eval();
            }
            else
            {
                dut.source_clk = 0;
                dut.eval();
            }
        }
    }

    if (
        results_sent !=
        static_cast<int>(source_results.size()))
    {
        std::cerr
            << "Not all results entered bridge: "
            << results_sent << "/"
            << source_results.size()
            << "\n";
        return 1;
    }

    if (
        results_received !=
        static_cast<int>(source_results.size()))
    {
        std::cerr
            << "Not all results crossed bridge: "
            << results_received << "/"
            << source_results.size()
            << "\n";
        return 1;
    }

    if (!pending_input.empty())
    {
        std::cerr << "Pending input results remain\n";
        return 1;
    }

    if (!expected_output.empty())
    {
        std::cerr << "Expected output results remain\n";
        return 1;
    }

    std::cout
        << "PASS: memory result CDC bridge verified\n";

    std::cout
        << "PASS: "
        << results_received
        << " COMMIT/ABORT results transferred in order\n";

    std::cout
        << "PASS: result type, slot and frame ID preserved\n";

    std::cout
        << "PASS: independent memory/source clocks exercised\n";

    return 0;
}
