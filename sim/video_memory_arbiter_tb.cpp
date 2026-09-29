#include <cstdint>
#include <deque>
#include <iostream>

#include "Vvideo_memory_arbiter.h"
#include "verilated.h"

static constexpr int TOTAL_WRITES = 500;
static constexpr int READ_BURSTS = 3;
static constexpr int READS_PER_BURST = 160;
static constexpr int TOTAL_READS =
    READ_BURSTS * READS_PER_BURST;

struct PendingWrite
{
    int due_cycle;
};

struct PendingRead
{
    int due_cycle;
    std::uint32_t data;
};

static void tick(Vvideo_memory_arbiter &dut)
{
    dut.clk = 1;
    dut.eval();

    dut.clk = 0;
    dut.eval();
}

static std::uint32_t read_data_for_address(
    std::uint32_t address)
{
    return (address * 0x10203041u) ^
           0xA55A5AA5u;
}

static bool read_burst_active(
    int cycle,
    int reads_accepted)
{
    /*
     * Start three read bursts at widely separated times.
     *
     * Once a burst starts, the read client keeps read_valid asserted
     * until all 160 reads in that burst have been accepted.
     */
    const int current_burst =
        reads_accepted / READS_PER_BURST;

    if (current_burst >= READ_BURSTS)
        return false;

    static constexpr int burst_start_cycles[READ_BURSTS] = {
        80,
        700,
        1350};

    return cycle >= burst_start_cycles[current_burst];
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_memory_arbiter dut;

    dut.clk = 0;
    dut.reset = 1;

    dut.write_valid = 0;
    dut.write_address = 0;
    dut.write_data = 0;

    dut.read_valid = 0;
    dut.read_address = 0;

    dut.memory_command_ready = 0;

    dut.memory_write_done = 0;

    dut.memory_read_response_valid = 0;
    dut.memory_read_response_data = 0;

    dut.eval();
    tick(dut);

    dut.reset = 0;
    dut.eval();

    int writes_accepted = 0;
    int writes_completed = 0;

    int reads_accepted = 0;
    int reads_completed = 0;

    int controller_stalls = 0;
    int write_priority_blocks = 0;

    int last_write_due = -1;
    int last_read_due = -1;

    std::deque<PendingWrite> pending_writes;
    std::deque<PendingRead> pending_reads;

    constexpr int MAX_CYCLES = 10000;

    for (int cycle = 0; cycle < MAX_CYCLES; ++cycle)
    {
        /*
         * Continuous write producer.
         */
        if (writes_accepted < TOTAL_WRITES)
        {
            dut.write_valid = 1;

            dut.write_address =
                1000 + writes_accepted;

            dut.write_data =
                0x80000000u |
                static_cast<std::uint32_t>(
                    writes_accepted);
        }
        else
        {
            dut.write_valid = 0;
            dut.write_address = 0;
            dut.write_data = 0;
        }

        /*
         * Read bursts.
         */
        if (
            reads_accepted < TOTAL_READS &&
            read_burst_active(
                cycle,
                reads_accepted))
        {
            dut.read_valid = 1;

            dut.read_address =
                50000 + reads_accepted;
        }
        else
        {
            dut.read_valid = 0;
            dut.read_address = 0;
        }

        /*
         * Irregular physical memory-controller backpressure.
         */
        dut.memory_command_ready =
            (cycle % 11 != 0) &&
            !(
                (cycle % 300) >= 190 &&
                (cycle % 300) < 205);

        if (!dut.memory_command_ready)
            ++controller_stalls;

        /*
         * Return at most one write completion and one read response
         * per cycle.
         */
        dut.memory_write_done = 0;

        if (
            !pending_writes.empty() &&
            pending_writes.front().due_cycle <= cycle)
        {
            dut.memory_write_done = 1;
        }

        dut.memory_read_response_valid = 0;
        dut.memory_read_response_data = 0;

        if (
            !pending_reads.empty() &&
            pending_reads.front().due_cycle <= cycle)
        {
            dut.memory_read_response_valid = 1;

            dut.memory_read_response_data =
                pending_reads.front().data;
        }

        dut.eval();

        /*
         * Core arbitration property:
         *
         * when a read is pending, a write must NEVER be selected.
         */
        if (dut.read_valid)
        {
            if (dut.write_ready)
            {
                std::cerr
                    << "Write marked ready while read had priority "
                    << "at cycle "
                    << cycle << "\n";
                return 1;
            }

            if (dut.memory_command_valid)
            {
                if (dut.memory_command_write)
                {
                    std::cerr
                        << "WRITE selected while READ was pending "
                        << "at cycle "
                        << cycle << "\n";
                    return 1;
                }

                if (
                    dut.memory_command_address !=
                    dut.read_address)
                {
                    std::cerr
                        << "Read address mismatch at cycle "
                        << cycle << "\n";
                    return 1;
                }
            }

            if (dut.write_valid)
                ++write_priority_blocks;
        }
        else if (dut.write_valid)
        {
            if (dut.memory_command_valid)
            {
                if (!dut.memory_command_write)
                {
                    std::cerr
                        << "READ selected with no read request "
                        << "at cycle "
                        << cycle << "\n";
                    return 1;
                }

                if (
                    dut.memory_command_address !=
                    dut.write_address)
                {
                    std::cerr
                        << "Write address mismatch at cycle "
                        << cycle << "\n";
                    return 1;
                }

                if (
                    dut.memory_command_write_data !=
                    dut.write_data)
                {
                    std::cerr
                        << "Write data mismatch at cycle "
                        << cycle << "\n";
                    return 1;
                }
            }
        }

        const bool read_accepted =
            dut.read_valid &&
            dut.read_ready;

        const bool write_accepted =
            dut.write_valid &&
            dut.write_ready;

        if (read_accepted && write_accepted)
        {
            std::cerr
                << "Read and write accepted simultaneously\n";
            return 1;
        }

        if (read_accepted)
        {
            const std::uint32_t address =
                dut.read_address;

            int due =
                cycle +
                4 +
                static_cast<int>(
                    address % 7);

            if (due <= last_read_due)
                due = last_read_due + 1;

            last_read_due = due;

            pending_reads.push_back({due,
                                     read_data_for_address(address)});

            ++reads_accepted;
        }

        if (write_accepted)
        {
            int due =
                cycle +
                3 +
                static_cast<int>(
                    dut.write_address % 5);

            if (due <= last_write_due)
                due = last_write_due + 1;

            last_write_due = due;

            pending_writes.push_back({due});

            ++writes_accepted;
        }

        /*
         * Check forwarded completion channels before clock edge.
         */
        if (
            dut.write_done !=
            dut.memory_write_done)
        {
            std::cerr
                << "write_done forwarding mismatch\n";
            return 1;
        }

        if (
            dut.read_response_valid !=
            dut.memory_read_response_valid)
        {
            std::cerr
                << "read_response_valid forwarding mismatch\n";
            return 1;
        }

        if (dut.read_response_valid)
        {
            if (pending_reads.empty())
            {
                std::cerr
                    << "Read response with no pending read\n";
                return 1;
            }

            const std::uint32_t expected =
                pending_reads.front().data;

            if (
                dut.read_response_data != expected)
            {
                std::cerr
                    << "Read response data mismatch: expected 0x"
                    << std::hex << expected
                    << ", got 0x"
                    << dut.read_response_data
                    << std::dec << "\n";
                return 1;
            }
        }

        tick(dut);

        if (dut.memory_write_done)
        {
            if (pending_writes.empty())
            {
                std::cerr
                    << "Write completion with no pending write\n";
                return 1;
            }

            pending_writes.pop_front();
            ++writes_completed;
        }

        if (dut.memory_read_response_valid)
        {
            pending_reads.pop_front();
            ++reads_completed;
        }

        if (
            writes_accepted == TOTAL_WRITES &&
            writes_completed == TOTAL_WRITES &&
            reads_accepted == TOTAL_READS &&
            reads_completed == TOTAL_READS &&
            pending_writes.empty() &&
            pending_reads.empty())
        {
            break;
        }
    }

    if (reads_accepted != TOTAL_READS)
    {
        std::cerr
            << "Read count mismatch: accepted "
            << reads_accepted << "/"
            << TOTAL_READS << "\n";
        return 1;
    }

    if (reads_completed != TOTAL_READS)
    {
        std::cerr
            << "Read completion mismatch: "
            << reads_completed << "/"
            << TOTAL_READS << "\n";
        return 1;
    }

    if (writes_accepted != TOTAL_WRITES)
    {
        std::cerr
            << "Write count mismatch: accepted "
            << writes_accepted << "/"
            << TOTAL_WRITES << "\n";
        return 1;
    }

    if (writes_completed != TOTAL_WRITES)
    {
        std::cerr
            << "Write completion mismatch: "
            << writes_completed << "/"
            << TOTAL_WRITES << "\n";
        return 1;
    }

    if (write_priority_blocks == 0)
    {
        std::cerr
            << "Read priority never blocked a pending write\n";
        return 1;
    }

    if (controller_stalls == 0)
    {
        std::cerr
            << "Physical controller backpressure not exercised\n";
        return 1;
    }

    std::cout
        << "PASS: video memory arbiter verified\n";

    std::cout
        << "PASS: "
        << TOTAL_READS
        << " reads completed with strict priority\n";

    std::cout
        << "PASS: "
        << TOTAL_WRITES
        << " writes resumed and completed without loss\n";

    std::cout
        << "PASS: pending writes blocked by reads on "
        << write_priority_blocks
        << " cycles\n";

    std::cout
        << "PASS: physical controller backpressure exercised\n";

    std::cout
        << "PASS: read/write completion channels preserved\n";

    return 0;
}
