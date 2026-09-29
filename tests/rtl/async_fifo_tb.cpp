#include <algorithm>
#include <cstdint>
#include <deque>
#include <iostream>

#include "Vasync_fifo.h"
#include "verilated.h"

static constexpr std::uint64_t WRITE_HALF_PERIOD = 5;
static constexpr std::uint64_t READ_HALF_PERIOD  = 7;

static constexpr std::uint64_t TOTAL_WORDS = 5000;

static std::uint64_t make_value(std::uint64_t index) {
    return ((index * 0x1F123BB5ULL) ^ 0x123456789ABULL)
        & ((1ULL << 50) - 1);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vasync_fifo dut;

    dut.write_clk = 0;
    dut.read_clk = 0;

    dut.write_reset = 1;
    dut.read_reset = 1;

    dut.write_valid = 0;
    dut.write_data = 0;

    dut.read_ready = 0;

    dut.eval();

    std::uint64_t now = 0;
    std::uint64_t next_write_toggle = WRITE_HALF_PERIOD;
    std::uint64_t next_read_toggle = READ_HALF_PERIOD;

    constexpr std::uint64_t RESET_RELEASE_TIME = 500;
    constexpr std::uint64_t MAX_TIME = 2000000;

    bool reset_released = false;

    std::uint64_t next_write_value = 0;
    std::uint64_t values_read = 0;

    std::deque<std::uint64_t> expected;

    bool saw_full_backpressure = false;
    bool saw_empty = false;
    bool saw_simultaneous_io = false;

    std::uint64_t read_cycle = 0;

    while (now < MAX_TIME && values_read < TOTAL_WORDS) {
        now = std::min(next_write_toggle, next_read_toggle);

        if (!reset_released && now >= RESET_RELEASE_TIME) {
            dut.write_reset = 0;
            dut.read_reset = 0;
            reset_released = true;
            dut.eval();
        }

        /*
         * WRITE CLOCK DOMAIN
         */
        if (now == next_write_toggle) {
            next_write_toggle += WRITE_HALF_PERIOD;

            if (dut.write_clk == 0) {
                if (
                    reset_released &&
                    next_write_value < TOTAL_WORDS
                ) {
                    /*
                     * Producer tries to write continuously.
                     * FIFO backpressure must be sufficient to prevent loss.
                     */
                    dut.write_valid = 1;
                    dut.write_data =
                        make_value(next_write_value);
                } else {
                    dut.write_valid = 0;
                    dut.write_data = 0;
                }

                dut.eval();

                const bool accepted =
                    dut.write_valid && dut.write_ready;

                if (dut.write_valid && !dut.write_ready)
                    saw_full_backpressure = true;

                if (accepted && dut.read_valid && dut.read_ready)
                    saw_simultaneous_io = true;

                dut.write_clk = 1;
                dut.eval();

                if (accepted) {
                    expected.push_back(
                        make_value(next_write_value)
                    );

                    ++next_write_value;
                }
            } else {
                dut.write_clk = 0;
                dut.eval();
            }
        }

        /*
         * READ CLOCK DOMAIN
         */
        if (now == next_read_toggle) {
            next_read_toggle += READ_HALF_PERIOD;

            if (dut.read_clk == 0) {
                if (reset_released) {
                    /*
                     * Deliberately irregular consumer:
                     *
                     * - normal short stalls
                     * - periodic long stalls that should fill the FIFO
                     */
                    const bool long_stall =
                        (read_cycle % 900) >= 250 &&
                        (read_cycle % 900) < 700;

                    const bool short_stall =
                        (read_cycle % 11) == 0 ||
                        (read_cycle % 17) == 0;

                    dut.read_ready =
                        !long_stall &&
                        !short_stall;
                } else {
                    dut.read_ready = 0;
                }

                dut.eval();

                if (!dut.read_valid)
                    saw_empty = true;

                const bool accepted =
                    dut.read_valid && dut.read_ready;

                std::uint64_t actual = 0;

                if (accepted) {
                    actual =
                        static_cast<std::uint64_t>(
                            dut.read_data
                        );

                    if (expected.empty()) {
                        std::cerr
                            << "FIFO produced data with no expected item\n";
                        return 1;
                    }

                    const std::uint64_t want =
                        expected.front();

                    if (actual != want) {
                        std::cerr
                            << "FIFO order/data mismatch at read "
                            << values_read
                            << ": expected 0x"
                            << std::hex << want
                            << ", got 0x"
                            << actual
                            << std::dec << "\n";
                        return 1;
                    }
                }

                dut.read_clk = 1;
                dut.eval();

                if (accepted) {
                    expected.pop_front();
                    ++values_read;
                }

                if (reset_released)
                    ++read_cycle;
            } else {
                dut.read_clk = 0;
                dut.eval();
            }
        }
    }

    if (values_read != TOTAL_WORDS) {
        std::cerr
            << "Timeout: read "
            << values_read
            << "/"
            << TOTAL_WORDS
            << " values\n";
        return 1;
    }

    if (next_write_value != TOTAL_WORDS) {
        std::cerr
            << "Producer did not enqueue all values: "
            << next_write_value
            << "/"
            << TOTAL_WORDS
            << "\n";
        return 1;
    }

    if (!expected.empty()) {
        std::cerr
            << "Expected queue still contains "
            << expected.size()
            << " values\n";
        return 1;
    }

    if (!saw_full_backpressure) {
        std::cerr
            << "Test never filled FIFO enough to assert backpressure\n";
        return 1;
    }

    if (!saw_empty) {
        std::cerr
            << "Test never observed an empty FIFO\n";
        return 1;
    }

    if (!saw_simultaneous_io) {
        std::cerr
            << "No simultaneous read/write activity observed\n";
        return 1;
    }

    std::cout << "PASS: asynchronous FIFO verified\n";
    std::cout
        << "PASS: "
        << TOTAL_WORDS
        << " values transferred in order without loss\n";
    std::cout << "PASS: full/backpressure condition exercised\n";
    std::cout << "PASS: empty condition exercised\n";
    std::cout << "PASS: simultaneous read/write activity observed\n";
    std::cout << "PASS: independent clock domains exercised\n";

    return 0;
}
