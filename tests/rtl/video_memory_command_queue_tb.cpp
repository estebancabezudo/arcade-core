#include <algorithm>
#include <cstdint>
#include <deque>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "Vvideo_memory_command_queue.h"
#include "verilated.h"

static constexpr std::uint64_t SOURCE_HALF_PERIOD = 5;
static constexpr std::uint64_t MEMORY_HALF_PERIOD = 7;

static constexpr int FRAMES = 8;
static constexpr int WRITES_PER_FRAME = 200;

static constexpr std::uint8_t COMMAND_WRITE = 0;
static constexpr std::uint8_t COMMAND_COMMIT = 1;
static constexpr std::uint8_t COMMAND_ABORT = 2;

struct SourceCommand
{
    std::uint8_t type;
    std::uint32_t address;
    std::uint32_t data;
    std::uint8_t slot;
    std::uint32_t frame_id;
};

struct PendingWrite
{
    int due_cycle;
    std::uint32_t address;
    std::uint32_t data;
};

static std::uint32_t make_data(
    std::uint32_t frame_id,
    std::uint32_t index)
{
    return (frame_id * 0x10203041u) ^
           (index * 0x01010101u) ^
           0xA5A55A5Au;
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_memory_command_queue dut;

    dut.source_clk = 0;
    dut.memory_clk = 0;

    dut.source_reset = 1;
    dut.memory_reset = 1;

    dut.source_command_valid = 0;
    dut.source_command_type = COMMAND_WRITE;

    dut.source_write_address = 0;
    dut.source_write_data = 0;

    dut.source_marker_slot = 0;
    dut.source_marker_frame_id = 0;

    dut.mem_write_ready = 0;
    dut.mem_write_done = 0;

    dut.eval();

    /*
     * Even-numbered frames commit normally.
     * Odd-numbered frames are deliberately aborted after only half
     * their writes have been queued.
     */
    std::vector<SourceCommand> commands;

    std::vector<int> expected_writes(FRAMES, 0);
    std::vector<bool> expected_commit(FRAMES, false);
    std::vector<bool> expected_abort(FRAMES, false);

    for (int frame = 0; frame < FRAMES; ++frame)
    {
        const std::uint8_t slot =
            static_cast<std::uint8_t>(frame % 4);

        const bool abort_frame =
            (frame & 1) != 0;

        const int writes =
            abort_frame
                ? WRITES_PER_FRAME / 2
                : WRITES_PER_FRAME;

        expected_writes[frame] = writes;
        expected_commit[frame] = !abort_frame;
        expected_abort[frame] = abort_frame;

        for (int i = 0; i < writes; ++i)
        {
            commands.push_back({COMMAND_WRITE,
                                static_cast<std::uint32_t>(
                                    slot * 38400 + i),
                                make_data(frame, i),
                                slot,
                                static_cast<std::uint32_t>(frame)});
        }

        commands.push_back({abort_frame
                                ? COMMAND_ABORT
                                : COMMAND_COMMIT,
                            0,
                            0,
                            slot,
                            static_cast<std::uint32_t>(frame)});
    }

    std::uint64_t now = 0;
    std::uint64_t next_source_toggle =
        SOURCE_HALF_PERIOD;
    std::uint64_t next_memory_toggle =
        MEMORY_HALF_PERIOD;

    constexpr std::uint64_t RESET_RELEASE_TIME = 500;
    constexpr std::uint64_t MAX_TIME = 5000000;

    bool reset_released = false;

    std::size_t command_index = 0;

    std::deque<PendingWrite> pending_writes;

    std::unordered_map<
        std::uint32_t,
        std::uint32_t>
        memory;

    std::vector<int> writes_completed_per_frame(
        FRAMES,
        0);

    std::vector<bool> commit_seen(FRAMES, false);
    std::vector<bool> abort_seen(FRAMES, false);

    int accepted_writes = 0;
    int completed_writes = 0;
    int commits_seen = 0;
    int aborts_seen = 0;

    int memory_cycle = 0;
    int last_due_cycle = -1;

    bool saw_source_backpressure = false;
    bool saw_memory_backpressure = false;
    bool saw_multiple_outstanding = false;

    while (
        now < MAX_TIME &&
        (commits_seen + aborts_seen) < FRAMES)
    {
        now = std::min(
            next_source_toggle,
            next_memory_toggle);

        if (
            !reset_released &&
            now >= RESET_RELEASE_TIME)
        {
            dut.source_reset = 0;
            dut.memory_reset = 0;

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
                if (
                    reset_released &&
                    command_index < commands.size())
                {
                    const auto &command =
                        commands[command_index];

                    dut.source_command_valid = 1;
                    dut.source_command_type =
                        command.type;

                    dut.source_write_address =
                        command.address;

                    dut.source_write_data =
                        command.data;

                    dut.source_marker_slot =
                        command.slot;

                    dut.source_marker_frame_id =
                        command.frame_id;
                }
                else
                {
                    dut.source_command_valid = 0;
                    dut.source_command_type =
                        COMMAND_WRITE;

                    dut.source_write_address = 0;
                    dut.source_write_data = 0;

                    dut.source_marker_slot = 0;
                    dut.source_marker_frame_id = 0;
                }

                dut.eval();

                const bool accepted =
                    dut.source_command_valid &&
                    dut.source_command_ready;

                if (
                    dut.source_command_valid &&
                    !dut.source_command_ready)
                {
                    saw_source_backpressure = true;
                }

                dut.source_clk = 1;
                dut.eval();

                if (accepted)
                    ++command_index;
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
                    const bool long_stall =
                        (memory_cycle % 400) >= 120 &&
                        (memory_cycle % 400) < 190;

                    const bool short_stall =
                        (memory_cycle % 13) == 0;

                    dut.mem_write_ready =
                        !long_stall &&
                        !short_stall;

                    if (!dut.mem_write_ready)
                        saw_memory_backpressure = true;

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
                            address % 11);

                    int due_cycle =
                        memory_cycle + latency;

                    if (due_cycle <= last_due_cycle)
                        due_cycle =
                            last_due_cycle + 1;

                    last_due_cycle = due_cycle;

                    pending_writes.push_back({due_cycle,
                                              address,
                                              data});

                    ++accepted_writes;

                    if (pending_writes.size() > 1)
                        saw_multiple_outstanding = true;
                }

                dut.memory_clk = 1;
                dut.eval();

                if (dut.protocol_error)
                {
                    std::cerr
                        << "Unexpected protocol_error\n";
                    return 1;
                }

                if (write_completed)
                {
                    if (pending_writes.empty())
                    {
                        std::cerr
                            << "Write completed with no "
                            << "pending write\n";
                        return 1;
                    }

                    const auto completed =
                        pending_writes.front();

                    pending_writes.pop_front();

                    memory[completed.address] =
                        completed.data;

                    ++completed_writes;

                    const int slot =
                        completed.address / 38400;

                    const int index_in_slot =
                        completed.address % 38400;

                    for (
                        int frame = 0;
                        frame < FRAMES;
                        ++frame)
                    {
                        if (
                            frame % 4 == slot &&
                            index_in_slot <
                                expected_writes[frame] &&
                            make_data(
                                frame,
                                index_in_slot) == completed.data)
                        {
                            ++writes_completed_per_frame[frame];
                            break;
                        }
                    }
                }

                if (dut.commit_valid)
                {
                    const int frame_id =
                        static_cast<int>(
                            dut.commit_frame_id);

                    if (
                        frame_id < 0 ||
                        frame_id >= FRAMES)
                    {
                        std::cerr
                            << "Invalid commit frame id "
                            << frame_id << "\n";
                        return 1;
                    }

                    if (!expected_commit[frame_id])
                    {
                        std::cerr
                            << "Unexpected commit for aborted "
                            << "frame "
                            << frame_id << "\n";
                        return 1;
                    }

                    if (
                        writes_completed_per_frame[frame_id] !=
                        expected_writes[frame_id])
                    {
                        std::cerr
                            << "Commit arrived too early for "
                            << "frame "
                            << frame_id
                            << ": completed "
                            << writes_completed_per_frame[frame_id]
                            << "/"
                            << expected_writes[frame_id]
                            << " writes\n";
                        return 1;
                    }

                    if (commit_seen[frame_id])
                    {
                        std::cerr
                            << "Duplicate commit for frame "
                            << frame_id << "\n";
                        return 1;
                    }

                    const std::uint8_t expected_slot =
                        static_cast<std::uint8_t>(
                            frame_id % 4);

                    if (
                        dut.commit_slot !=
                        expected_slot)
                    {
                        std::cerr
                            << "Commit slot mismatch for "
                            << "frame "
                            << frame_id << "\n";
                        return 1;
                    }

                    commit_seen[frame_id] = true;
                    ++commits_seen;
                }

                if (dut.abort_valid)
                {
                    const int frame_id =
                        static_cast<int>(
                            dut.abort_frame_id);

                    if (
                        frame_id < 0 ||
                        frame_id >= FRAMES)
                    {
                        std::cerr
                            << "Invalid abort frame id "
                            << frame_id << "\n";
                        return 1;
                    }

                    if (!expected_abort[frame_id])
                    {
                        std::cerr
                            << "Unexpected abort for committed "
                            << "frame "
                            << frame_id << "\n";
                        return 1;
                    }

                    if (
                        writes_completed_per_frame[frame_id] !=
                        expected_writes[frame_id])
                    {
                        std::cerr
                            << "Abort arrived too early for "
                            << "frame "
                            << frame_id
                            << ": completed "
                            << writes_completed_per_frame[frame_id]
                            << "/"
                            << expected_writes[frame_id]
                            << " writes\n";
                        return 1;
                    }

                    if (abort_seen[frame_id])
                    {
                        std::cerr
                            << "Duplicate abort for frame "
                            << frame_id << "\n";
                        return 1;
                    }

                    const std::uint8_t expected_slot =
                        static_cast<std::uint8_t>(
                            frame_id % 4);

                    if (
                        dut.abort_slot !=
                        expected_slot)
                    {
                        std::cerr
                            << "Abort slot mismatch for frame "
                            << frame_id << "\n";
                        return 1;
                    }

                    abort_seen[frame_id] = true;
                    ++aborts_seen;
                }

                if (reset_released)
                    ++memory_cycle;
            }
            else
            {
                dut.memory_clk = 0;
                dut.eval();
            }
        }
    }

    if (command_index != commands.size())
    {
        std::cerr
            << "Not all source commands were accepted: "
            << command_index
            << "/"
            << commands.size()
            << "\n";
        return 1;
    }

    int expected_total_writes = 0;

    for (int frame = 0; frame < FRAMES; ++frame)
        expected_total_writes +=
            expected_writes[frame];

    if (accepted_writes != expected_total_writes)
    {
        std::cerr
            << "Accepted write count mismatch: expected "
            << expected_total_writes
            << ", got "
            << accepted_writes << "\n";
        return 1;
    }

    if (completed_writes != expected_total_writes)
    {
        std::cerr
            << "Completed write count mismatch: expected "
            << expected_total_writes
            << ", got "
            << completed_writes << "\n";
        return 1;
    }

    for (int frame = 0; frame < FRAMES; ++frame)
    {
        if (
            writes_completed_per_frame[frame] !=
            expected_writes[frame])
        {
            std::cerr
                << "Frame "
                << frame
                << " completed write count mismatch\n";
            return 1;
        }

        if (
            expected_commit[frame] &&
            !commit_seen[frame])
        {
            std::cerr
                << "Missing commit for frame "
                << frame << "\n";
            return 1;
        }

        if (
            expected_abort[frame] &&
            !abort_seen[frame])
        {
            std::cerr
                << "Missing abort for frame "
                << frame << "\n";
            return 1;
        }
    }

    if (!saw_memory_backpressure)
    {
        std::cerr
            << "Memory backpressure was never exercised\n";
        return 1;
    }

    if (!saw_multiple_outstanding)
    {
        std::cerr
            << "No multiple outstanding writes observed\n";
        return 1;
    }

    std::cout
        << "PASS: WRITE/COMMIT/ABORT queue verified\n";

    std::cout
        << "PASS: "
        << accepted_writes
        << " writes completed without loss\n";

    std::cout
        << "PASS: "
        << commits_seen
        << " commits emitted after all preceding writes\n";

    std::cout
        << "PASS: "
        << aborts_seen
        << " aborts emitted after all partial writes\n";

    std::cout
        << "PASS: aborted frames were never committed\n";

    std::cout
        << "PASS: committed frames were never aborted\n";

    std::cout
        << "PASS: memory backpressure exercised\n";

    std::cout
        << "PASS: multiple outstanding writes exercised\n";

    if (saw_source_backpressure)
    {
        std::cout
            << "INFO: source FIFO backpressure was exercised\n";
    }
    else
    {
        std::cout
            << "INFO: source FIFO did not fill in this test\n";
    }

    return 0;
}
