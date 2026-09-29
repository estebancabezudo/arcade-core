#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <vector>

#include <SDL2/SDL.h>

#include "Vvideo_system.h"
#include "verilated.h"

extern "C" {
#include "pacman_machine.h"
#include "pacman_rom_loader.h"
}

static constexpr int SOURCE_WIDTH =
    PACMAN_VISIBLE_HEIGHT; /* 224 after rotation */

static constexpr int SOURCE_HEIGHT =
    PACMAN_VISIBLE_WIDTH;  /* 288 after rotation */

static constexpr int SOURCE_PIXELS =
    SOURCE_WIDTH * SOURCE_HEIGHT;

static constexpr int WORDS_PER_FRAME =
    SOURCE_PIXELS / 2;

static constexpr int FRAMEBUFFER_WORDS =
    4 * WORDS_PER_FRAME;

static constexpr int HDMI_WIDTH = 1280;
static constexpr int HDMI_HEIGHT = 720;

/*
 * Pac-Man pixel clock: 6.144 MHz
 * HDMI pixel clock:    74.25 MHz
 *
 * 74.25 / 6.144 = 12375 / 1024
 *
 * These are arbitrary simulation time units preserving
 * the exact clock ratio.
 */
static constexpr std::uint64_t SOURCE_HALF_PERIOD = 12375;
static constexpr std::uint64_t VIDEO_HALF_PERIOD = 1024;

/*
 * Original Pac-Man raster:
 *
 *   384 x 264 pixels
 *   101376 pixel clocks/frame
 *   approximately 60.606 Hz
 *
 * The host adapter presents the rotated visible image as
 * a packed 224 x 288 stream during the active portion of
 * each source frame.
 */
static constexpr std::uint64_t SOURCE_FRAME_CYCLES =
    static_cast<std::uint64_t>(
        PACMAN_RASTER_WIDTH) *
    PACMAN_RASTER_HEIGHT;

static constexpr std::uint64_t SOURCE_ACTIVE_START = 3;

static constexpr std::uint64_t SOURCE_ACTIVE_END =
    SOURCE_ACTIVE_START +
    SOURCE_PIXELS;

struct PendingWrite
{
    std::uint64_t due_cycle;
    std::uint32_t address;
    std::uint32_t data;
};

struct PendingRead
{
    std::uint64_t due_cycle;
    std::uint32_t data;
};

struct HostControls
{
    SDL_Scancode last_direction =
        SDL_SCANCODE_UNKNOWN;

    std::uint32_t coin_frames = 0;

    bool start1 = false;
    bool start2 = false;
};

struct ViewerState
{
    bool running = true;
    bool fullscreen = false;

    std::uint64_t presented_frames = 0;
    std::uint64_t source_frames = 0;

    std::uint64_t source_drops = 0;
    std::uint64_t capture_aborts = 0;
    std::uint64_t underflows = 0;

    bool previous_capture_failed = false;
    bool previous_underflow = false;
};

static std::uint16_t rgb565(
    PacmanRgb color)
{
    return
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(
                color.r >> 3)
             << 11) |
            (static_cast<std::uint16_t>(
                color.g >> 2)
             << 5) |
            static_cast<std::uint16_t>(
                color.b >> 3));
}

static bool run_one_machine_frame(
    PacmanMachine &machine)
{
    const std::uint64_t target_frame =
        machine.video_timing.frame + 1u;

    while (
        machine.video_timing.frame <
        target_frame)
    {
        const std::uint32_t t_states =
            pacman_machine_step(
                &machine);

        if (t_states == 0u)
        {
            std::cerr
                << "CPU stopped unexpectedly at PC="
                << std::hex
                << machine.cpu_board.cpu.pc
                << std::dec
                << "\n";

            return false;
        }
    }

    return true;
}

static void capture_rotated_machine_frame(
    PacmanMachine &machine,
    std::vector<std::uint16_t> &frame)
{
    /*
     * Pac-Man board-visible raster:
     *
     *     288 x 224
     *
     * Cabinet monitor orientation:
     *
     *     224 x 288
     *
     * Rotate clockwise exactly as pacman-sim does.
     */
    for (
        std::uint16_t y = 0;
        y < PACMAN_VISIBLE_HEIGHT;
        ++y)
    {
        for (
            std::uint16_t x = 0;
            x < PACMAN_VISIBLE_WIDTH;
            ++x)
        {
            const PacmanRgb color =
                pacman_machine_video_pixel(
                    &machine,
                    x,
                    y);

            const std::uint16_t display_x =
                static_cast<std::uint16_t>(
                    PACMAN_VISIBLE_HEIGHT -
                    1u -
                    y);

            const std::uint16_t display_y =
                x;

            frame[
                static_cast<std::size_t>(
                    display_y) *
                    SOURCE_WIDTH +
                display_x] =
                rgb565(color);
        }
    }
}

static std::uint8_t direction_bit(
    SDL_Scancode scancode)
{
    switch (scancode)
    {
        case SDL_SCANCODE_UP:
            return 0x01u;

        case SDL_SCANCODE_LEFT:
            return 0x02u;

        case SDL_SCANCODE_RIGHT:
            return 0x04u;

        case SDL_SCANCODE_DOWN:
            return 0x08u;

        default:
            return 0u;
    }
}

static std::uint8_t current_direction(
    const HostControls &controls)
{
    const std::uint8_t *keys =
        SDL_GetKeyboardState(nullptr);

    /*
     * Original cabinet uses a 4-way restrictor.
     *
     * Prefer the most recently pressed direction while
     * it remains held, preventing keyboard diagonals.
     */
    if (
        controls.last_direction !=
            SDL_SCANCODE_UNKNOWN &&
        keys[controls.last_direction])
    {
        return direction_bit(
            controls.last_direction);
    }

    static constexpr SDL_Scancode directions[] = {
        SDL_SCANCODE_UP,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_DOWN
    };

    for (const SDL_Scancode direction : directions)
    {
        if (keys[direction])
        {
            return direction_bit(
                direction);
        }
    }

    return 0u;
}

static void apply_controls(
    PacmanMachine &machine,
    const HostControls &controls)
{
    std::uint8_t input0 = 0xffu;
    std::uint8_t input1 = 0xffu;

    input0 &=
        static_cast<std::uint8_t>(
            ~current_direction(
                controls));

    if (controls.coin_frames > 0u)
    {
        input0 &=
            static_cast<std::uint8_t>(
                ~0x20u);
    }

    if (controls.start1)
    {
        input1 &=
            static_cast<std::uint8_t>(
                ~0x20u);
    }

    if (controls.start2)
    {
        input1 &=
            static_cast<std::uint8_t>(
                ~0x40u);
    }

    machine.input0 = input0;
    machine.input1 = input1;
}

static void controls_frame_complete(
    HostControls &controls)
{
    if (controls.coin_frames > 0u)
    {
        --controls.coin_frames;
    }
}

static void process_events(
    SDL_Window *window,
    ViewerState &viewer,
    HostControls &controls)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            viewer.running = false;
            continue;
        }

        if (event.type == SDL_KEYDOWN)
        {
            const SDL_Scancode scancode =
                event.key.keysym.scancode;

            if (
                event.key.keysym.sym ==
                SDLK_ESCAPE)
            {
                viewer.running = false;
                continue;
            }

            if (
                event.key.keysym.sym ==
                    SDLK_f &&
                event.key.repeat == 0)
            {
                viewer.fullscreen =
                    !viewer.fullscreen;

                SDL_SetWindowFullscreen(
                    window,
                    viewer.fullscreen
                        ? SDL_WINDOW_FULLSCREEN_DESKTOP
                        : 0);

                continue;
            }

            if (
                event.key.repeat == 0 &&
                direction_bit(scancode) != 0u)
            {
                controls.last_direction =
                    scancode;
            }

            if (event.key.repeat != 0)
            {
                continue;
            }

            switch (scancode)
            {
                case SDL_SCANCODE_5:
                case SDL_SCANCODE_KP_5:
                    controls.coin_frames = 2u;
                    break;

                case SDL_SCANCODE_1:
                case SDL_SCANCODE_KP_1:
                    controls.start1 = true;
                    break;

                case SDL_SCANCODE_2:
                case SDL_SCANCODE_KP_2:
                    controls.start2 = true;
                    break;

                default:
                    break;
            }
        }

        if (event.type == SDL_KEYUP)
        {
            switch (event.key.keysym.scancode)
            {
                case SDL_SCANCODE_1:
                case SDL_SCANCODE_KP_1:
                    controls.start1 = false;
                    break;

                case SDL_SCANCODE_2:
                case SDL_SCANCODE_KP_2:
                    controls.start2 = false;
                    break;

                default:
                    break;
            }
        }
    }
}

static bool initialize_sdl(
    SDL_Window *&window,
    SDL_Renderer *&renderer,
    SDL_Texture *&texture)
{
    if (
        SDL_Init(
            SDL_INIT_VIDEO |
            SDL_INIT_EVENTS) != 0)
    {
        std::cerr
            << "SDL_Init failed: "
            << SDL_GetError()
            << "\n";

        return false;
    }

    SDL_SetHint(
        SDL_HINT_RENDER_SCALE_QUALITY,
        "0");

    window =
        SDL_CreateWindow(
            "arcade-core | Pac-Man through FPGA video pipeline",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            HDMI_WIDTH,
            HDMI_HEIGHT,
            SDL_WINDOW_SHOWN |
                SDL_WINDOW_RESIZABLE);

    if (window == nullptr)
    {
        std::cerr
            << "SDL_CreateWindow failed: "
            << SDL_GetError()
            << "\n";

        return false;
    }

    renderer =
        SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED);

    if (renderer == nullptr)
    {
        renderer =
            SDL_CreateRenderer(
                window,
                -1,
                SDL_RENDERER_SOFTWARE);
    }

    if (renderer == nullptr)
    {
        std::cerr
            << "SDL_CreateRenderer failed: "
            << SDL_GetError()
            << "\n";

        return false;
    }

    SDL_RenderSetLogicalSize(
        renderer,
        HDMI_WIDTH,
        HDMI_HEIGHT);

    texture =
        SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING,
            HDMI_WIDTH,
            HDMI_HEIGHT);

    if (texture == nullptr)
    {
        std::cerr
            << "SDL_CreateTexture failed: "
            << SDL_GetError()
            << "\n";

        return false;
    }

    return true;
}

static void print_expected_roms(
    const char *directory)
{
    std::cerr
        << "\nExpected ROM files in "
        << directory
        << ":\n"
        << "  pacman.6e\n"
        << "  pacman.6f\n"
        << "  pacman.6h\n"
        << "  pacman.6j\n"
        << "  pacman.5e\n"
        << "  pacman.5f\n"
        << "  82s123.7f\n"
        << "  82s126.4a\n"
        << "  82s126.1m\n"
        << "  82s126.3m\n";
}

int main(
    int argc,
    char **argv)
{
    Verilated::commandArgs(
        argc,
        argv);

    const char *rom_directory =
        argc >= 2
            ? argv[1]
            : "roms/pacman";

    PacmanRomSet romset;

    if (
        !host_load_pacman_romset(
            rom_directory,
            &romset))
    {
        std::cerr
            << "Could not load Pac-Man ROM set.\n";

        print_expected_roms(
            rom_directory);

        return EXIT_FAILURE;
    }

    PacmanMachine machine;

    if (
        !pacman_machine_init(
            &machine,

            romset.program,
            sizeof(romset.program),

            romset.tile_rom,
            sizeof(romset.tile_rom),

            romset.sprite_rom,
            sizeof(romset.sprite_rom),

            romset.palette_prom,
            sizeof(romset.palette_prom),

            romset.color_lookup_prom,
            sizeof(romset.color_lookup_prom),

            romset.sound_waveform_prom,
            sizeof(romset.sound_waveform_prom),

            romset.sound_timing_prom,
            sizeof(romset.sound_timing_prom)))
    {
        std::cerr
            << "Could not initialize Pac-Man machine.\n";

        return EXIT_FAILURE;
    }

    /*
     * Standard upright Pac-Man settings:
     *
     * 1 coin / 1 credit
     * 3 lives
     * 10000 bonus
     * normal difficulty
     */
    machine.dsw1 = 0xc9u;

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;

    if (
        !initialize_sdl(
            window,
            renderer,
            texture))
    {
        if (texture != nullptr)
        {
            SDL_DestroyTexture(
                texture);
        }

        if (renderer != nullptr)
        {
            SDL_DestroyRenderer(
                renderer);
        }

        if (window != nullptr)
        {
            SDL_DestroyWindow(
                window);
        }

        SDL_Quit();

        return EXIT_FAILURE;
    }

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
        FRAMEBUFFER_WORDS,
        0);

    std::deque<PendingWrite> pending_writes;
    std::deque<PendingRead> pending_reads;

    std::vector<std::uint16_t> source_frame(
        SOURCE_PIXELS,
        0);

    std::vector<std::uint16_t> display_frame(
        HDMI_WIDTH * HDMI_HEIGHT,
        0);

    ViewerState viewer;
    HostControls controls;

    std::uint64_t simulation_time = 0;

    std::uint64_t next_source_toggle =
        SOURCE_HALF_PERIOD;

    std::uint64_t next_video_toggle =
        VIDEO_HALF_PERIOD;

    std::uint64_t source_cycle = 0;
    std::uint64_t video_cycle = 0;

    std::uint64_t last_write_due = 0;
    std::uint64_t last_read_due = 0;

    /*
     * Resets are synchronous in both RTL clock domains.
     *
     * Keep reset asserted long enough for the slower source
     * domain to observe several rising edges before release.
     */
    constexpr std::uint64_t RESET_RELEASE_TIME =
        SOURCE_HALF_PERIOD * 8u;

    bool reset_released = false;
    bool collecting_frame = false;

    std::uint64_t collected_pixels = 0;

    auto fps_window_start =
        std::chrono::steady_clock::now();

    std::uint64_t fps_window_frames = 0;
    double simulation_fps = 0.0;

    auto next_present_time =
        std::chrono::steady_clock::now();

    while (
        viewer.running &&
        !Verilated::gotFinish())
    {
        simulation_time =
            std::min(
                next_source_toggle,
                next_video_toggle);

        if (
            !reset_released &&
            simulation_time >=
                RESET_RELEASE_TIME)
        {
            dut.source_reset = 0;
            dut.video_reset = 0;

            reset_released = true;
            dut.eval();
        }

        /*
         * Pac-Man / RP2350-facing source clock domain.
         */
        if (
            simulation_time ==
            next_source_toggle)
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
                    const std::uint64_t phase =
                        source_cycle %
                        SOURCE_FRAME_CYCLES;

                    if (phase == 0)
                    {
                        apply_controls(
                            machine,
                            controls);

                        if (
                            !run_one_machine_frame(
                                machine))
                        {
                            viewer.running = false;
                        }
                        else
                        {
                            controls_frame_complete(
                                controls);

                            capture_rotated_machine_frame(
                                machine,
                                source_frame);

                            dut.source_frame_start = 1;

                            viewer.source_frames =
                                machine.video_timing.frame;
                        }
                    }

                    if (
                        phase >=
                            SOURCE_ACTIVE_START &&
                        phase <
                            SOURCE_ACTIVE_END)
                    {
                        const std::uint64_t index =
                            phase -
                            SOURCE_ACTIVE_START;

                        const std::uint32_t x =
                            static_cast<std::uint32_t>(
                                index %
                                SOURCE_WIDTH);

                        const std::uint32_t y =
                            static_cast<std::uint32_t>(
                                index /
                                SOURCE_WIDTH);

                        dut.pixel_valid = 1;

                        dut.pixel_x = x;
                        dut.pixel_y = y;

                        dut.pixel_rgb565 =
                            source_frame[index];

                        if (
                            index ==
                            SOURCE_PIXELS - 1)
                        {
                            dut.source_frame_complete = 1;
                        }
                    }
                }

                dut.eval();

                dut.source_clk = 1;
                dut.eval();

                if (reset_released)
                {
                    if (dut.source_frame_dropped)
                    {
                        ++viewer.source_drops;
                    }

                    if (
                        dut.capture_failed &&
                        !viewer.previous_capture_failed)
                    {
                        ++viewer.capture_aborts;
                    }

                    viewer.previous_capture_failed =
                        dut.capture_failed;

                    if (
                        dut.manager_error ||
                        dut.memory_queue_error ||
                        dut.result_bridge_error)
                    {
                        std::cerr
                            << "Video pipeline diagnostic error"
                            << " manager="
                            << static_cast<unsigned>(dut.manager_error)
                            << " queue="
                            << static_cast<unsigned>(dut.memory_queue_error)
                            << " bridge="
                            << static_cast<unsigned>(dut.result_bridge_error)
                            << " capture_failed="
                            << static_cast<unsigned>(dut.capture_failed)
                            << " source_cycle="
                            << source_cycle
                            << " machine_frame="
                            << machine.video_timing.frame
                            << "\n";

                        viewer.running = false;
                    }

                    ++source_cycle;
                }
            }
            else
            {
                dut.source_clk = 0;
                dut.eval();
            }
        }

        /*
         * HDMI + framebuffer memory domain.
         */
        if (
            simulation_time ==
            next_video_toggle)
        {
            next_video_toggle +=
                VIDEO_HALF_PERIOD;

            if (dut.video_clk == 0)
            {
                dut.memory_write_done = 0;

                dut.memory_read_response_valid = 0;
                dut.memory_read_response_data = 0;

                /*
                 * Deterministic SDRAM-like command backpressure.
                 */
                const bool short_stall =
                    (video_cycle % 97u) == 0u;

                const bool refresh_like_stall =
                    ((video_cycle % 12000u) >= 9000u &&
                     (video_cycle % 12000u) < 9012u);

                dut.memory_command_ready =
                    !short_stall &&
                    !refresh_like_stall;

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

                /*
                 * Capture one complete 1280 x 720 HDMI frame.
                 */
                if (
                    !collecting_frame &&
                    dut.framebuffer_valid &&
                    dut.hdmi_active &&
                    dut.hdmi_x == 0 &&
                    dut.hdmi_y == 0)
                {
                    collecting_frame = true;
                    collected_pixels = 0;
                }

                if (
                    collecting_frame &&
                    dut.hdmi_active)
                {
                    const int x =
                        static_cast<int>(
                            dut.hdmi_x);

                    const int y =
                        static_cast<int>(
                            dut.hdmi_y);

                    if (
                        x >= 0 &&
                        x < HDMI_WIDTH &&
                        y >= 0 &&
                        y < HDMI_HEIGHT)
                    {
                        display_frame[
                            y *
                                HDMI_WIDTH +
                            x] =
                            dut.hdmi_rgb565;

                        ++collected_pixels;
                    }

                    if (
                        x == HDMI_WIDTH - 1 &&
                        y == HDMI_HEIGHT - 1)
                    {
                        if (
                            collected_pixels !=
                            static_cast<std::uint64_t>(
                                HDMI_WIDTH) *
                                HDMI_HEIGHT)
                        {
                            std::cerr
                                << "Incomplete HDMI frame: "
                                << collected_pixels
                                << " pixels\n";

                            viewer.running = false;
                        }
                        else
                        {
                            SDL_UpdateTexture(
                                texture,
                                nullptr,
                                display_frame.data(),
                                HDMI_WIDTH *
                                    sizeof(std::uint16_t));

                            SDL_RenderClear(
                                renderer);

                            SDL_RenderCopy(
                                renderer,
                                texture,
                                nullptr,
                                nullptr);

                            SDL_RenderPresent(
                                renderer);

                            ++viewer.presented_frames;
                            ++fps_window_frames;

                            process_events(
                                window,
                                viewer,
                                controls);

                            const auto now_wall =
                                std::chrono::steady_clock::now();

                            const double elapsed =
                                std::chrono::duration<double>(
                                    now_wall -
                                    fps_window_start)
                                    .count();

                            if (elapsed >= 1.0)
                            {
                                simulation_fps =
                                    static_cast<double>(
                                        fps_window_frames) /
                                    elapsed;

                                fps_window_frames = 0;
                                fps_window_start =
                                    now_wall;
                            }

                            char title[320];

                            std::snprintf(
                                title,
                                sizeof(title),
                                "arcade-core | Pac-Man -> FPGA CRT | "
                                "%.1f fps | machine %llu | "
                                "HDMI framebuffer %u | PC=%04x | "
                                "drops %llu | aborts %llu | "
                                "underflows %llu | "
                                "arrows | 5 coin | 1 start | "
                                "F fullscreen | Esc quit",
                                simulation_fps,
                                static_cast<unsigned long long>(
                                    viewer.source_frames),
                                static_cast<unsigned>(
                                    dut.framebuffer_id),
                                static_cast<unsigned>(
                                    machine.cpu_board.cpu.pc),
                                static_cast<unsigned long long>(
                                    viewer.source_drops),
                                static_cast<unsigned long long>(
                                    viewer.capture_aborts),
                                static_cast<unsigned long long>(
                                    viewer.underflows));

                            SDL_SetWindowTitle(
                                window,
                                title);

                            /*
                             * Never present faster than 60 Hz.
                             * Simulation may run more slowly.
                             */
                            next_present_time +=
                                std::chrono::nanoseconds(
                                    16666667);

                            if (
                                next_present_time >
                                now_wall)
                            {
                                std::this_thread::sleep_until(
                                    next_present_time);
                            }
                            else
                            {
                                next_present_time =
                                    now_wall;
                            }
                        }

                        collecting_frame = false;
                        collected_pixels = 0;
                    }
                }

                if (
                    dut.line_underflow &&
                    !viewer.previous_underflow)
                {
                    ++viewer.underflows;
                }

                viewer.previous_underflow =
                    dut.line_underflow;

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
                            << "Framebuffer address out of range: "
                            << address
                            << "\n";

                        viewer.running = false;
                    }
                    else if (
                        dut.memory_command_write)
                    {
                        std::uint64_t due =
                            video_cycle +
                            3u +
                            (address % 5u);

                        if (due <= last_write_due)
                        {
                            due =
                                last_write_due + 1u;
                        }

                        last_write_due = due;

                        pending_writes.push_back(
                            {
                                due,
                                address,
                                dut.memory_command_write_data
                            });
                    }
                    else
                    {
                        std::uint64_t due =
                            video_cycle +
                            2u +
                            (address % 7u);

                        if (due <= last_read_due)
                        {
                            due =
                                last_read_due + 1u;
                        }

                        last_read_due = due;

                        pending_reads.push_back(
                            {
                                due,
                                memory[address]
                            });
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

                        viewer.running = false;
                    }
                    else
                    {
                        const PendingWrite completed =
                            pending_writes.front();

                        pending_writes.pop_front();

                        memory[completed.address] =
                            completed.data;
                    }
                }

                if (
                    dut.memory_read_response_valid)
                {
                    if (pending_reads.empty())
                    {
                        std::cerr
                            << "Read response with no pending read\n";

                        viewer.running = false;
                    }
                    else
                    {
                        pending_reads.pop_front();
                    }
                }

                /*
                 * Poll before the first HDMI frame is available.
                 */
                if (
                    (video_cycle & 0x3fffu) == 0u)
                {
                    process_events(
                        window,
                        viewer,
                        controls);
                }

                ++video_cycle;
            }
            else
            {
                dut.video_clk = 0;
                dut.eval();
            }
        }
    }

    dut.final();

    SDL_DestroyTexture(
        texture);

    SDL_DestroyRenderer(
        renderer);

    SDL_DestroyWindow(
        window);

    SDL_Quit();

    std::cout
        << "Machine frames: "
        << viewer.source_frames
        << "\n";

    std::cout
        << "Presented HDMI frames: "
        << viewer.presented_frames
        << "\n";

    std::cout
        << "Source drops: "
        << viewer.source_drops
        << "\n";

    std::cout
        << "Capture aborts: "
        << viewer.capture_aborts
        << "\n";

    std::cout
        << "Line underflows: "
        << viewer.underflows
        << "\n";

    return viewer.running
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
