#include <algorithm>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Vvideo_system.h"
#include "verilated.h"

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
static constexpr int SOURCE_PIXELS =
    SOURCE_WIDTH * SOURCE_HEIGHT;
static constexpr int WORDS_PER_FRAME =
    SOURCE_PIXELS / 2;
static constexpr int WORDS_PER_LINE = SOURCE_WIDTH / 2;

static constexpr int HDMI_WIDTH = 1280;
static constexpr int HDMI_HEIGHT = 720;
static constexpr int CONTENT_WIDTH = SOURCE_WIDTH * SCALE;
static constexpr int CONTENT_HEIGHT = SOURCE_HEIGHT * SCALE;
static constexpr int CONTENT_X_START =
    (HDMI_WIDTH - CONTENT_WIDTH) / 2;
static constexpr int CONTENT_Y_START =
    (HDMI_HEIGHT - CONTENT_HEIGHT) / 2;

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
    FrameStart,
    Gap,
    Pixels,
    Finished
};

static std::uint16_t source_pixel(int x, int y)
{
    /* Bright maze walls on black, inspired by early arcade geometry. */
    const bool outer_wall =
        ((x == 8 || x == 9 || x == 310 || x == 311) &&
         y >= 8 && y <= 231) ||
        ((y == 8 || y == 9 || y == 230 || y == 231) &&
         x >= 8 && x <= 311);

    const bool inner_wall =
        ((y == 48 || y == 49 || y == 144 || y == 145) &&
         x >= 40 && x <= 279) ||
        ((x == 64 || x == 65 || x == 254 || x == 255) &&
         y >= 48 && y <= 145) ||
        ((y == 96 || y == 97) &&
         ((x >= 64 && x <= 135) || (x >= 184 && x <= 255)));

    if (outer_wall || inner_wall)
        return 0x07FF;

    /* One-pixel dots exercise beam width and transition handling. */
    if ((x % 16) == 8 && (y % 16) == 8)
        return ((x / 16) & 1) ? 0xFFFF : 0x07FF;

    /* Small synthetic sprites with saturated colors. */
    if (x >= 137 && x <= 150 && y >= 108 && y <= 121)
        return 0xF800;

    if (x >= 169 && x <= 182 && y >= 108 && y <= 121)
        return 0xFFE0;

    if (x >= 151 && x <= 168 && y >= 168 && y <= 185)
        return 0xF81F;

    /* Uniform swatches cover saturated colors, white and medium gray. */
    if (y >= 200 && y <= 219)
    {
        switch ((x - 24) / 34)
        {
        case 0: return (x >= 24) ? 0xF800 : 0x0000;
        case 1: return 0x07E0;
        case 2: return 0x001F;
        case 3: return 0xFFFF;
        case 4: return 0x07FF;
        case 5: return 0xF81F;
        case 6: return 0xFFE0;
        case 7: return 0x7BEF;
        default: return 0x0000;
        }
    }

    return 0x0000;
}

static std::uint16_t scale_rgb565(
    std::uint16_t pixel,
    std::uint16_t intensity)
{
    const std::uint16_t r =
        (pixel >> 11) & 0x1F;

    const std::uint16_t g =
        (pixel >> 5) & 0x3F;

    const std::uint16_t b =
        pixel & 0x1F;

    const std::uint16_t ro =
        (r * intensity) >> 8;

    const std::uint16_t go =
        (g * intensity) >> 8;

    const std::uint16_t bo =
        (b * intensity) >> 8;

    return static_cast<std::uint16_t>(
        (ro << 11) |
        (go << 5) |
        bo);
}

static std::uint16_t apply_beam(
    std::uint16_t pixel,
    int repeat)
{
    if (SCALE != 3)
        return pixel;

    const std::uint16_t r5 = (pixel >> 11) & 0x1F;
    const std::uint16_t g6 = (pixel >> 5) & 0x3F;
    const std::uint16_t b5 = pixel & 0x1F;
    const std::uint16_t r6 = (r5 << 1) | (r5 >> 4);
    const std::uint16_t b6 = (b5 << 1) | (b5 >> 4);
    const std::uint16_t luminance = (r6 + 2 * g6 + b6) >> 2;

    if (repeat == 0)
        return scale_rgb565(pixel, 154 + luminance);

    if (repeat == 1)
        return pixel;

    return scale_rgb565(
        pixel,
        179 + ((luminance * 13 + 8) >> 4));
}

static std::uint16_t apply_bleed(
    std::uint16_t left,
    std::uint16_t center,
    std::uint16_t right)
{
    const auto channel = [left, center, right](int shift, int mask) {
        const int l = (left >> shift) & mask;
        const int c = (center >> shift) & mask;
        const int r = (right >> shift) & mask;
        return (l * 8 + c * 240 + r * 8) >> 8;
    };

    return static_cast<std::uint16_t>(
        (channel(11, 0x1F) << 11) |
        (channel(5, 0x3F) << 5) |
        channel(0, 0x1F));
}

static std::uint16_t apply_phosphor(
    std::uint16_t pixel,
    int content_x)
{
    const int phase = content_x % 3;
    const std::uint16_t r_weight = (phase == 0) ? 256 : 241;
    const std::uint16_t g_weight = (phase == 1) ? 256 : 241;
    const std::uint16_t b_weight = (phase == 2) ? 256 : 241;

    const std::uint16_t r = ((pixel >> 11) & 0x1F) * r_weight >> 8;
    const std::uint16_t g = ((pixel >> 5) & 0x3F) * g_weight >> 8;
    const std::uint16_t b = (pixel & 0x1F) * b_weight >> 8;

    return static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
}

static std::uint16_t beam_pixel(int content_x, int y)
{
    return apply_beam(
        source_pixel(
            content_x / SCALE,
            (y - CONTENT_Y_START) / SCALE),
        (y - CONTENT_Y_START) % SCALE);
}

static std::uint16_t expected_crt_pixel(int x, int y)
{
    if (
        x < CONTENT_X_START ||
        x >= CONTENT_X_START + CONTENT_WIDTH ||
        y < CONTENT_Y_START ||
        y >= CONTENT_Y_START + CONTENT_HEIGHT)
        return 0;

    const int content_x = x - CONTENT_X_START;
    const std::uint16_t left =
        (content_x == 0) ? 0 : beam_pixel(content_x - 1, y);
    const std::uint16_t center = beam_pixel(content_x, y);
    const std::uint16_t right =
        (content_x == CONTENT_WIDTH - 1)
            ? 0
            : beam_pixel(content_x + 1, y);

    return apply_phosphor(
        apply_bleed(left, center, right),
        content_x);
}

static void write_rgb565(
    std::ofstream &output,
    std::uint16_t pixel)
{
    const std::uint8_t r5 =
        (pixel >> 11) & 0x1F;

    const std::uint8_t g6 =
        (pixel >> 5) & 0x3F;

    const std::uint8_t b5 =
        pixel & 0x1F;

    const std::uint8_t rgb[3] = {
        static_cast<std::uint8_t>(
            (r5 << 3) | (r5 >> 2)),
        static_cast<std::uint8_t>(
            (g6 << 2) | (g6 >> 4)),
        static_cast<std::uint8_t>(
            (b5 << 3) | (b5 >> 2))};

    output.write(
        reinterpret_cast<const char *>(rgb),
        sizeof(rgb));
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);

    Vvideo_system dut;

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

    dut.memory_command_ready = 0;

    dut.memory_write_done = 0;

    dut.memory_read_response_valid = 0;
    dut.memory_read_response_data = 0;

    dut.eval();

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
    constexpr std::uint64_t MAX_TIME = 25000000;

    bool reset_released = false;

    SourceState source_state =
        SourceState::FrameStart;

    int source_gap = 0;
    int source_pixel_index = 0;
    int source_pixels_sent = 0;

    int video_cycle = 0;

    int last_write_due = -1;
    int last_read_due = -1;

    int writes_accepted = 0;
    int writes_completed = 0;

    int reads_accepted = 0;
    int reads_completed = 0;

    bool framebuffer_selected = false;
    bool output_frame_started = false;

    std::uint64_t output_pixels = 0;

    std::ofstream crt_output;
    std::ofstream plain_output;
    const std::string geometry_tag =
        std::to_string(SOURCE_WIDTH) + "x" +
        std::to_string(SOURCE_HEIGHT) + "_x" +
        std::to_string(SCALE);
    const std::string geometry_suffix =
        (SOURCE_WIDTH == 320 &&
         SOURCE_HEIGHT == 240 &&
         SCALE == 3)
            ? ""
            : "_" + geometry_tag;
    const std::string crt_output_path =
        "build/video_system/frame_system_crt" +
        geometry_suffix + "_1280x720.ppm";
    const std::string plain_output_path =
        "build/video_system/frame_system_plain" +
        geometry_suffix + "_1280x720.ppm";

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

                if (reset_released)
                {
                    switch (source_state)
                    {
                    case SourceState::FrameStart:
                        dut.source_frame_start = 1;
                        break;

                    case SourceState::Gap:
                        break;

                    case SourceState::Pixels:
                    {
                        const int x =
                            source_pixel_index %
                            SOURCE_WIDTH;

                        const int y =
                            source_pixel_index /
                            SOURCE_WIDTH;

                        dut.pixel_valid = 1;
                        dut.pixel_x = x;
                        dut.pixel_y = y;

                        dut.pixel_rgb565 =
                            source_pixel(x, y);

                        if (
                            source_pixel_index ==
                            SOURCE_PIXELS - 1)
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
                            << "Video system diagnostic error\n";
                        return 1;
                    }

                    if (dut.source_frame_dropped)
                    {
                        std::cerr
                            << "Source frame was dropped\n";
                        return 1;
                    }

                    if (dut.capture_failed)
                    {
                        std::cerr
                            << "Unexpected framebuffer capture failure\n";
                        return 1;
                    }

                    switch (source_state)
                    {
                    case SourceState::FrameStart:
                        source_gap = 0;
                        source_state =
                            SourceState::Gap;
                        break;

                    case SourceState::Gap:
                        ++source_gap;

                        if (source_gap >= 2)
                        {
                            source_pixel_index = 0;
                            source_state =
                                SourceState::Pixels;
                        }

                        break;

                    case SourceState::Pixels:
                        ++source_pixel_index;
                        ++source_pixels_sent;

                        if (
                            source_pixel_index >=
                            SOURCE_PIXELS)
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

        if (now == next_video_toggle)
        {
            next_video_toggle +=
                VIDEO_HALF_PERIOD;

            if (dut.video_clk == 0)
            {
                dut.memory_write_done = 0;

                dut.memory_read_response_valid = 0;
                dut.memory_read_response_data = 0;

                dut.memory_command_ready =
                    (video_cycle % 31) != 0 &&
                    !(
                        (video_cycle % 1000) >= 600 &&
                        (video_cycle % 1000) < 620);

                if (
                    !pending_writes.empty() &&
                    pending_writes.front().due_cycle <=
                        video_cycle)
                {
                    dut.memory_write_done = 1;
                }

                if (
                    !pending_reads.empty() &&
                    pending_reads.front().due_cycle <=
                        video_cycle)
                {
                    dut.memory_read_response_valid = 1;

                    dut.memory_read_response_data =
                        pending_reads.front().data;
                }

                dut.eval();

                if (
                    dut.manager_error ||
                    dut.memory_queue_error ||
                    dut.result_bridge_error)
                {
                    std::cerr
                        << "Video system diagnostic error "
                        << "at video cycle "
                        << video_cycle
                        << "\n";
                    return 1;
                }

                if (
                    !output_frame_started &&
                    dut.framebuffer_valid &&
                    dut.hdmi_x == 0 &&
                    dut.hdmi_y == 0)
                {
                    crt_output.open(
                        crt_output_path,
                        std::ios::binary);

                    plain_output.open(
                        plain_output_path,
                        std::ios::binary);

                    if (!crt_output || !plain_output)
                    {
                        std::cerr << "Could not create output PPMs\n";
                        return 1;
                    }

                    crt_output
                        << "P6\n"
                        << HDMI_WIDTH
                        << " "
                        << HDMI_HEIGHT
                        << "\n255\n";

                    plain_output
                        << "P6\n"
                        << HDMI_WIDTH
                        << " "
                        << HDMI_HEIGHT
                        << "\n255\n";

                    output_frame_started = true;
                    framebuffer_selected = true;
                }

                if (
                    output_frame_started &&
                    dut.hdmi_active)
                {
                    const int x =
                        dut.hdmi_x;

                    const int y =
                        dut.hdmi_y;

                    const std::uint16_t expected =
                        expected_crt_pixel(x, y);

                    std::uint16_t plain = 0;

                    if (
                        x >= CONTENT_X_START &&
                        x < CONTENT_X_START + CONTENT_WIDTH &&
                        y >= CONTENT_Y_START &&
                        y < CONTENT_Y_START + CONTENT_HEIGHT)
                    {
                        const int source_x =
                            (x - CONTENT_X_START) / SCALE;

                        const int source_y =
                            (y - CONTENT_Y_START) / SCALE;

                        plain = source_pixel(source_x, source_y);
                    }

                    if (
                        dut.hdmi_rgb565 !=
                        expected)
                    {
                        std::cerr
                            << "Pixel mismatch at HDMI "
                            << x << ","
                            << y
                            << ": expected 0x"
                            << std::hex
                            << expected
                            << ", got 0x"
                            << dut.hdmi_rgb565
                            << std::dec
                            << "\n";
                        return 1;
                    }

                    write_rgb565(
                        crt_output,
                        dut.hdmi_rgb565);

                    write_rgb565(
                        plain_output,
                        plain);

                    ++output_pixels;
                }

                if (
                    output_frame_started &&
                    dut.line_underflow)
                {
                    std::cerr
                        << "HDMI line underflow at "
                        << dut.hdmi_x
                        << ","
                        << dut.hdmi_y
                        << "\n";
                    return 1;
                }

                const bool command_accepted =
                    dut.memory_command_valid &&
                    dut.memory_command_ready;

                if (command_accepted)
                {
                    const std::uint32_t address =
                        dut.memory_command_address;

                    if (address >= memory.size())
                    {
                        std::cerr
                            << "Memory address outside framebuffer: "
                            << address
                            << "\n";
                        return 1;
                    }

                    if (dut.memory_command_write)
                    {
                        int due =
                            video_cycle +
                            3 +
                            static_cast<int>(
                                address % 5);

                        if (due <= last_write_due)
                            due = last_write_due + 1;

                        last_write_due = due;

                        pending_writes.push_back({due,
                                                  address,
                                                  dut.memory_command_write_data});

                        ++writes_accepted;
                    }
                    else
                    {
                        int due =
                            video_cycle +
                            2 +
                            static_cast<int>(
                                address % 7);

                        if (due <= last_read_due)
                            due = last_read_due + 1;

                        last_read_due = due;

                        pending_reads.push_back({due,
                                                 memory[address]});

                        ++reads_accepted;
                    }
                }

                dut.video_clk = 1;
                dut.eval();

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

                    ++writes_completed;
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
                    ++reads_completed;
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
            output_frame_started &&
            output_pixels ==
                static_cast<std::uint64_t>(
                    HDMI_WIDTH) *
                    HDMI_HEIGHT)
        {
            break;
        }
    }

    if (crt_output.is_open())
        crt_output.close();

    if (plain_output.is_open())
        plain_output.close();

    if (
        source_pixels_sent !=
        SOURCE_PIXELS)
    {
        std::cerr
            << "Source raster interrupted: expected "
            << SOURCE_PIXELS
            << " pixels, got "
            << source_pixels_sent
            << "\n";
        return 1;
    }

    if (!framebuffer_selected)
    {
        std::cerr
            << "Committed framebuffer was never selected\n";
        return 1;
    }

    const std::uint64_t expected_output_pixels =
        static_cast<std::uint64_t>(
            HDMI_WIDTH) *
        HDMI_HEIGHT;

    if (
        output_pixels !=
        expected_output_pixels)
    {
        std::cerr
            << "Output frame incomplete: expected "
            << expected_output_pixels
            << " pixels, got "
            << output_pixels
            << "\n";
        return 1;
    }

    if (
        writes_accepted !=
            WORDS_PER_FRAME ||
        writes_completed !=
            WORDS_PER_FRAME)
    {
        std::cerr
            << "Framebuffer write count mismatch\n";
        return 1;
    }

    if (
        reads_accepted !=
            SOURCE_HEIGHT * WORDS_PER_LINE ||
        reads_completed !=
            SOURCE_HEIGHT * WORDS_PER_LINE)
    {
        std::cerr
            << "Framebuffer read count mismatch\n";
        return 1;
    }

    if (dut.line_underflow)
    {
        std::cerr
            << "Frame ended with line_underflow\n";
        return 1;
    }

    std::cout
        << "PASS: complete video system CRT pipeline verified\n";

    std::cout
        << "PASS: beam, bleed, mask, borders and repeats verified pixel-by-pixel\n";

    std::cout
        << "PASS: "
        << writes_completed
        << " framebuffer writes completed\n";

    std::cout
        << "PASS: "
        << reads_completed
        << " framebuffer reads completed\n";

    std::cout
        << "PASS: all "
        << output_pixels
        << " active HDMI pixels verified\n";

    std::cout
        << "Generated: "
        << plain_output_path << " and "
        << crt_output_path << "\n";

    return 0;
}
