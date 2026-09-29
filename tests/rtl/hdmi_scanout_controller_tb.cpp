#include <cstdint>
#include <iostream>

#include "Vhdmi_scanout_controller.h"
#include "verilated.h"

static constexpr int H_ACTIVE = 1280;
static constexpr int H_TOTAL = 1650;

static constexpr int V_ACTIVE = 720;
static constexpr int V_TOTAL = 750;

#ifndef TEST_SOURCE_WIDTH
#define TEST_SOURCE_WIDTH 320
#endif

#ifndef TEST_SOURCE_HEIGHT
#define TEST_SOURCE_HEIGHT 240
#endif

#ifndef TEST_SCALE
#define TEST_SCALE 3
#endif

static constexpr int SOURCE_WIDTH = TEST_SOURCE_WIDTH;
static constexpr int SOURCE_HEIGHT = TEST_SOURCE_HEIGHT;
static constexpr int SCALE = TEST_SCALE;
static constexpr int CONTENT_WIDTH = SOURCE_WIDTH * SCALE;
static constexpr int CONTENT_HEIGHT = SOURCE_HEIGHT * SCALE;
static constexpr int CONTENT_X_START =
    (H_ACTIVE - CONTENT_WIDTH) / 2;
static constexpr int CONTENT_X_END =
    CONTENT_X_START + CONTENT_WIDTH;
static constexpr int CONTENT_Y_START =
    (V_ACTIVE - CONTENT_HEIGHT) / 2;
static constexpr int CONTENT_Y_END =
    CONTENT_Y_START + CONTENT_HEIGHT;

static void tick(Vhdmi_scanout_controller &dut)
{
    dut.clk = 1;
    dut.eval();

    dut.clk = 0;
    dut.eval();
}

static std::uint16_t source_pixel(int y, int x)
{
    return static_cast<std::uint16_t>(
        ((y * 0x1234u) ^
         (x * 0x01F3u) ^
         0xA55Au) &
        0xFFFFu);
}

static int fetch_latency(int y)
{
    /*
     * Deliberately variable latency.
     *
     * Maximum:
     *   220 + 6 * 40 = 460 clocks
     *
     * Available between source lines:
     *   690 clocks
     */
    return 220 + (y % 7) * 40;
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vhdmi_scanout_controller dut;

    dut.clk = 0;
    dut.reset = 1;

    dut.timing_active = 0;
    dut.timing_x = 0;
    dut.timing_y = 0;

    dut.frame_select_pulse = 0;
    dut.frame_valid = 0;

    dut.line_busy = 0;
    dut.line_valid = 0;
    dut.line_y = 0;
    dut.line_rgb565 = 0;

    dut.eval();
    tick(dut);

    dut.reset = 0;
    dut.frame_valid = 1;
    dut.eval();

    bool fetch_in_progress = false;
    int fetch_due_cycle = 0;
    int fetch_y = 0;

    bool cache_valid = false;
    int cache_y = 0;

    int cycle = 0;

    int fetch_count = 0;
    int release_count = 0;

    std::uint64_t content_pixels = 0;

    auto run_pixel = [&](int x, int y, bool frame_select) -> bool
    {
        /*
         * Complete a simulated line fetch when its variable latency expires.
         */
        if (
            fetch_in_progress &&
            cycle >= fetch_due_cycle)
        {
            fetch_in_progress = false;
            cache_valid = true;
            cache_y = fetch_y;
        }

        dut.timing_x = x;
        dut.timing_y = y;

        dut.timing_active =
            (x < H_ACTIVE) &&
            (y < V_ACTIVE);

        dut.frame_select_pulse =
            frame_select;

        dut.line_busy =
            fetch_in_progress;

        dut.line_valid =
            cache_valid;

        dut.line_y =
            cache_valid
                ? cache_y
                : 0;

        /*
         * First eval determines which cached source X the renderer requests.
         */
        dut.line_rgb565 = 0;
        dut.eval();

        if (cache_valid)
        {
            dut.line_rgb565 =
                source_pixel(
                    cache_y,
                    dut.line_read_x);

            dut.eval();
        }

        const bool fetch_start =
            dut.line_fetch_start;

        const bool release =
            dut.line_release;

        /*
         * Verify fetch requests are exactly 0..SOURCE_HEIGHT-1 in order.
         */
        if (fetch_start)
        {
            if (fetch_in_progress)
            {
                std::cerr
                    << "Fetch requested while another fetch is busy\n";
                return false;
            }

            if (cache_valid)
            {
                std::cerr
                    << "Fetch requested while old line remains valid\n";
                return false;
            }

            if (fetch_count >= SOURCE_HEIGHT)
            {
                std::cerr
                    << "Too many line fetch requests\n";
                return false;
            }

            if (dut.line_fetch_y != fetch_count)
            {
                std::cerr
                    << "Fetch Y mismatch: expected "
                    << fetch_count
                    << ", got "
                    << static_cast<int>(
                           dut.line_fetch_y)
                    << "\n";
                return false;
            }
        }

        /*
         * Verify visible content.
         */
        if (y < V_ACTIVE)
        {
            const bool expected_content =
                x >= CONTENT_X_START &&
                x < CONTENT_X_END &&
                y >= CONTENT_Y_START &&
                y < CONTENT_Y_END;

            if (
                dut.content_active !=
                expected_content)
            {
                std::cerr
                    << "content_active mismatch at "
                    << x << "," << y
                    << "\n";
                return false;
            }

            if (expected_content)
            {
                const int expected_content_x =
                    x - CONTENT_X_START;

                const int expected_source_x =
                    expected_content_x / SCALE;

                const int expected_source_y =
                    (y - CONTENT_Y_START) / SCALE;

                const int expected_h_repeat =
                    expected_content_x % SCALE;

                const int expected_v_repeat =
                    (y - CONTENT_Y_START) % SCALE;

                if (
                    dut.content_x !=
                    expected_content_x)
                {
                    std::cerr
                        << "content_x mismatch at "
                        << x << "," << y
                        << "\n";
                    return false;
                }

                if (
                    dut.horizontal_repeat !=
                    expected_h_repeat)
                {
                    std::cerr
                        << "horizontal_repeat mismatch at "
                        << x << "," << y
                        << "\n";
                    return false;
                }

                if (
                    dut.vertical_repeat !=
                    expected_v_repeat)
                {
                    std::cerr
                        << "vertical_repeat mismatch at "
                        << x << "," << y
                        << "\n";
                    return false;
                }

                if (
                    dut.line_read_x !=
                    expected_source_x)
                {
                    std::cerr
                        << "line_read_x mismatch at "
                        << x << "," << y
                        << ": expected "
                        << expected_source_x
                        << ", got "
                        << dut.line_read_x
                        << "\n";
                    return false;
                }

                const std::uint16_t expected_pixel =
                    source_pixel(
                        expected_source_y,
                        expected_source_x);

                if (
                    dut.content_rgb565 !=
                    expected_pixel)
                {
                    std::cerr
                        << "Pixel mismatch at HDMI "
                        << x << "," << y
                        << " -> source "
                        << expected_source_x
                        << "," << expected_source_y
                        << "\n";
                    return false;
                }

                ++content_pixels;
            }
            else
            {
                if (dut.content_rgb565 != 0)
                {
                    std::cerr
                        << "Non-black pixel outside content at "
                        << x << "," << y
                        << "\n";
                    return false;
                }
            }
        }

        /*
         * release_line must happen exactly after the third output of each
         * source line.
         */
        const bool expected_release =
            y >= CONTENT_Y_START &&
            y < CONTENT_Y_END &&
            x == CONTENT_X_END - 1 &&
            ((y - CONTENT_Y_START) % SCALE) == SCALE - 1;

        if (release != expected_release)
        {
            std::cerr
                << "line_release mismatch at "
                << x << "," << y
                << "\n";
            return false;
        }

        if (release)
        {
            if (!cache_valid)
            {
                std::cerr
                    << "Released a line with no valid cache\n";
                return false;
            }

            const int expected_y =
                (y - CONTENT_Y_START) / SCALE;

            if (cache_y != expected_y)
            {
                std::cerr
                    << "Released wrong cached line: expected "
                    << expected_y
                    << ", got "
                    << cache_y
                    << "\n";
                return false;
            }
        }

        tick(dut);

        /*
         * Underflow is sticky for the frame. Detect it immediately.
         */
        if (dut.line_underflow)
        {
            std::cerr
                << "line_underflow at HDMI "
                << x << "," << y
                << "\n";
            return false;
        }

        /*
         * Model the external line-fetcher state transitions after the edge.
         */
        if (release)
        {
            cache_valid = false;
            ++release_count;
        }

        if (fetch_start)
        {
            fetch_in_progress = true;
            fetch_y =
                dut.line_fetch_y;

            fetch_due_cycle =
                cycle +
                fetch_latency(fetch_y);

            ++fetch_count;
        }

        ++cycle;

        return true;
    };

    /*
     * Simulate the vertical blanking preceding the visible frame.
     *
     * Select the framebuffer at y=720,x=0. This gives almost 30 complete
     * blank lines to prefetch source line 0.
     */
    for (int y = 720; y < V_TOTAL; ++y)
    {
        for (int x = 0; x < H_TOTAL; ++x)
        {
            const bool frame_select =
                (y == 720) &&
                (x == 0);

            if (!run_pixel(x, y, frame_select))
                return 1;
        }
    }

    if (!cache_valid || cache_y != 0)
    {
        std::cerr
            << "Source line 0 was not prefetched during vertical blanking\n";
        return 1;
    }

    /*
     * Simulate all 720 visible HDMI lines.
     */
    for (int y = 0; y < V_ACTIVE; ++y)
    {
        for (int x = 0; x < H_TOTAL; ++x)
        {
            if (!run_pixel(x, y, false))
                return 1;
        }
    }

    if (dut.line_underflow)
    {
        std::cerr
            << "Frame finished with line_underflow\n";
        return 1;
    }

    if (fetch_count != SOURCE_HEIGHT)
    {
        std::cerr
            << "Expected "
            << SOURCE_HEIGHT
            << " line fetches, got "
            << fetch_count
            << "\n";
        return 1;
    }

    if (release_count != SOURCE_HEIGHT)
    {
        std::cerr
            << "Expected "
            << SOURCE_HEIGHT
            << " line releases, got "
            << release_count
            << "\n";
        return 1;
    }

    const std::uint64_t expected_content_pixels =
        static_cast<std::uint64_t>(CONTENT_WIDTH) *
        CONTENT_HEIGHT;

    if (
        content_pixels !=
        expected_content_pixels)
    {
        std::cerr
            << "Content pixel count mismatch: expected "
            << expected_content_pixels
            << ", got "
            << content_pixels
            << "\n";
        return 1;
    }

    std::cout
        << "PASS: HDMI scanout controller verified\n";

    std::cout
        << "PASS: " << SOURCE_HEIGHT
        << " source lines prefetched and released exactly once\n";

    std::cout
        << "PASS: " << SOURCE_WIDTH << "x" << SOURCE_HEIGHT
        << " -> " << CONTENT_WIDTH << "x" << CONTENT_HEIGHT
        << " scaling verified under HDMI timing\n";

    std::cout
        << "PASS: variable line-fetch latency up to 460 clocks tolerated\n";

    std::cout
        << "PASS: no line underflow across complete 720p frame\n";

    std::cout
        << "PASS: "
        << content_pixels
        << " visible content pixels verified\n";

    return 0;
}
