#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "pacman_machine.h"
#include "pacman_rom_loader.h"

enum {
    SOURCE_WIDTH = PACMAN_VISIBLE_HEIGHT,
    SOURCE_HEIGHT = PACMAN_VISIBLE_WIDTH,

    HDMI_WIDTH = 1280,
    HDMI_HEIGHT = 720,

    SCALE = 2,

    CONTENT_WIDTH = SOURCE_WIDTH * SCALE,
    CONTENT_HEIGHT = SOURCE_HEIGHT * SCALE,

    CONTENT_X = (HDMI_WIDTH - CONTENT_WIDTH) / 2,
    CONTENT_Y = (HDMI_HEIGHT - CONTENT_HEIGHT) / 2
};

typedef struct {
    SDL_Scancode last_direction;

    uint32_t coin_frames;

    bool start1;
    bool start2;
} HostControls;

static uint16_t rgb565(
    PacmanRgb color
) {
    return (uint16_t)(
        ((uint16_t)(color.r >> 3) << 11) |
        ((uint16_t)(color.g >> 2) << 5) |
        (uint16_t)(color.b >> 3)
    );
}

static bool run_one_frame(
    PacmanMachine *machine
) {
    const uint64_t target_frame =
        machine->video_timing.frame + 1u;

    while (
        machine->video_timing.frame <
        target_frame
    ) {
        const uint32_t t_states =
            pacman_machine_step(
                machine
            );

        if (t_states == 0u) {
            fprintf(
                stderr,
                "CPU stopped unexpectedly at PC=%04x\n",
                machine->cpu_board.cpu.pc
            );

            return false;
        }
    }

    return true;
}

static void build_rotated_frame(
    PacmanMachine *machine,
    uint16_t *pixels
) {
    for (
        uint16_t y = 0;
        y < PACMAN_VISIBLE_HEIGHT;
        y++
    ) {
        for (
            uint16_t x = 0;
            x < PACMAN_VISIBLE_WIDTH;
            x++
        ) {
            const PacmanRgb color =
                pacman_machine_video_pixel(
                    machine,
                    x,
                    y
                );

            const uint16_t display_x =
                (uint16_t)(
                    PACMAN_VISIBLE_HEIGHT -
                    1u -
                    y
                );

            const uint16_t display_y =
                x;

            pixels[
                (size_t)display_y *
                    SOURCE_WIDTH +
                display_x
            ] =
                rgb565(
                    color
                );
        }
    }
}

static uint8_t direction_bit(
    SDL_Scancode scancode
) {
    switch (scancode) {
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

static uint8_t current_direction(
    const HostControls *controls
) {
    const uint8_t *keys =
        SDL_GetKeyboardState(
            NULL
        );

    /*
     * Pac-Man uses a 4-way joystick.
     */
    if (
        controls->last_direction !=
            SDL_SCANCODE_UNKNOWN &&
        keys[controls->last_direction]
    ) {
        return direction_bit(
            controls->last_direction
        );
    }

    static const SDL_Scancode directions[] = {
        SDL_SCANCODE_UP,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_DOWN
    };

    for (
        size_t i = 0;
        i <
            sizeof(directions) /
            sizeof(directions[0]);
        i++
    ) {
        if (keys[directions[i]]) {
            return direction_bit(
                directions[i]
            );
        }
    }

    return 0u;
}

static void apply_controls(
    PacmanMachine *machine,
    const HostControls *controls
) {
    uint8_t input0 = 0xffu;
    uint8_t input1 = 0xffu;

    input0 &=
        (uint8_t)~current_direction(
            controls
        );

    if (controls->coin_frames > 0u) {
        input0 &= (uint8_t)~0x20u;
    }

    if (controls->start1) {
        input1 &= (uint8_t)~0x20u;
    }

    if (controls->start2) {
        input1 &= (uint8_t)~0x40u;
    }

    machine->input0 = input0;
    machine->input1 = input1;
}

static void controls_frame_complete(
    HostControls *controls
) {
    if (controls->coin_frames > 0u) {
        controls->coin_frames--;
    }
}

static bool process_events(
    SDL_Window *window,
    bool *fullscreen,
    HostControls *controls
) {
    SDL_Event event;

    while (
        SDL_PollEvent(
            &event
        )
    ) {
        if (event.type == SDL_QUIT) {
            return false;
        }

        if (event.type == SDL_KEYDOWN) {
            const SDL_Scancode scancode =
                event.key.keysym.scancode;

            if (
                event.key.keysym.sym ==
                SDLK_ESCAPE
            ) {
                return false;
            }

            if (
                event.key.keysym.sym ==
                    SDLK_f &&
                event.key.repeat == 0
            ) {
                *fullscreen =
                    !*fullscreen;

                SDL_SetWindowFullscreen(
                    window,
                    *fullscreen
                        ? SDL_WINDOW_FULLSCREEN_DESKTOP
                        : 0
                );

                continue;
            }

            if (
                event.key.repeat == 0 &&
                direction_bit(scancode) != 0u
            ) {
                controls->last_direction =
                    scancode;
            }

            if (event.key.repeat != 0) {
                continue;
            }

            switch (scancode) {
                case SDL_SCANCODE_5:
                case SDL_SCANCODE_KP_5:
                    controls->coin_frames = 2u;
                    break;

                case SDL_SCANCODE_1:
                case SDL_SCANCODE_KP_1:
                    controls->start1 = true;
                    break;

                case SDL_SCANCODE_2:
                case SDL_SCANCODE_KP_2:
                    controls->start2 = true;
                    break;

                default:
                    break;
            }
        }

        if (event.type == SDL_KEYUP) {
            switch (
                event.key.keysym.scancode
            ) {
                case SDL_SCANCODE_1:
                case SDL_SCANCODE_KP_1:
                    controls->start1 = false;
                    break;

                case SDL_SCANCODE_2:
                case SDL_SCANCODE_KP_2:
                    controls->start2 = false;
                    break;

                default:
                    break;
            }
        }
    }

    return true;
}

static void print_expected_roms(
    const char *directory
) {
    fprintf(
        stderr,
        "\nExpected ROM files in %s:\n"
        "  pacman.6e\n"
        "  pacman.6f\n"
        "  pacman.6h\n"
        "  pacman.6j\n"
        "  pacman.5e\n"
        "  pacman.5f\n"
        "  82s123.7f\n"
        "  82s126.4a\n"
        "  82s126.1m\n"
        "  82s126.3m\n",
        directory
    );
}

int main(
    int argc,
    char **argv
) {
    const char *rom_directory =
        argc >= 2
            ? argv[1]
            : "roms/pacman";

    PacmanRomSet romset;

    if (
        !host_load_pacman_romset(
            rom_directory,
            &romset
        )
    ) {
        fprintf(
            stderr,
            "Could not load Pac-Man ROM set.\n"
        );

        print_expected_roms(
            rom_directory
        );

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
            sizeof(romset.sound_timing_prom)
        )
    ) {
        fprintf(
            stderr,
            "Could not initialize Pac-Man machine.\n"
        );

        return EXIT_FAILURE;
    }

    machine.dsw1 = 0xc9u;

    if (
        SDL_Init(
            SDL_INIT_VIDEO |
            SDL_INIT_EVENTS
        ) != 0
    ) {
        fprintf(
            stderr,
            "SDL_Init failed: %s\n",
            SDL_GetError()
        );

        return EXIT_FAILURE;
    }

    SDL_SetHint(
        SDL_HINT_RENDER_SCALE_QUALITY,
        "0"
    );

    SDL_Window *window =
        SDL_CreateWindow(
            "arcade-core | Pac-Man | 1280x720 fast viewer",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            HDMI_WIDTH,
            HDMI_HEIGHT,
            SDL_WINDOW_SHOWN |
            SDL_WINDOW_RESIZABLE
        );

    if (window == NULL) {
        fprintf(
            stderr,
            "SDL_CreateWindow failed: %s\n",
            SDL_GetError()
        );

        SDL_Quit();

        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED
        );

    if (renderer == NULL) {
        renderer =
            SDL_CreateRenderer(
                window,
                -1,
                SDL_RENDERER_SOFTWARE
            );
    }

    if (renderer == NULL) {
        fprintf(
            stderr,
            "SDL_CreateRenderer failed: %s\n",
            SDL_GetError()
        );

        SDL_DestroyWindow(
            window
        );

        SDL_Quit();

        return EXIT_FAILURE;
    }

    SDL_RenderSetLogicalSize(
        renderer,
        HDMI_WIDTH,
        HDMI_HEIGHT
    );

    SDL_RenderSetIntegerScale(
        renderer,
        SDL_TRUE
    );

    SDL_SetRenderDrawColor(
        renderer,
        0,
        0,
        0,
        255
    );

    SDL_Texture *texture =
        SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING,
            SOURCE_WIDTH,
            SOURCE_HEIGHT
        );

    if (texture == NULL) {
        fprintf(
            stderr,
            "SDL_CreateTexture failed: %s\n",
            SDL_GetError()
        );

        SDL_DestroyRenderer(
            renderer
        );

        SDL_DestroyWindow(
            window
        );

        SDL_Quit();

        return EXIT_FAILURE;
    }

    uint16_t *pixels =
        malloc(
            sizeof(uint16_t) *
            SOURCE_WIDTH *
            SOURCE_HEIGHT
        );

    if (pixels == NULL) {
        fprintf(
            stderr,
            "Could not allocate frame buffer.\n"
        );

        SDL_DestroyTexture(
            texture
        );

        SDL_DestroyRenderer(
            renderer
        );

        SDL_DestroyWindow(
            window
        );

        SDL_Quit();

        return EXIT_FAILURE;
    }

    const SDL_Rect content_rect = {
        CONTENT_X,
        CONTENT_Y,
        CONTENT_WIDTH,
        CONTENT_HEIGHT
    };

    const uint64_t performance_frequency =
        SDL_GetPerformanceFrequency();

    const double frame_seconds =
        (
            (double)PACMAN_RASTER_WIDTH *
            (double)PACMAN_RASTER_HEIGHT
        ) /
        (double)PACMAN_PIXEL_CLOCK_HZ;

    bool running = true;
    bool fullscreen = false;

    HostControls controls = {
        .last_direction =
            SDL_SCANCODE_UNKNOWN,

        .coin_frames = 0,

        .start1 = false,
        .start2 = false
    };

    uint64_t displayed_frames = 0;

    while (running) {
        const uint64_t frame_start =
            SDL_GetPerformanceCounter();

        running =
            process_events(
                window,
                &fullscreen,
                &controls
            );

        if (!running) {
            break;
        }

        apply_controls(
            &machine,
            &controls
        );

        if (
            !run_one_frame(
                &machine
            )
        ) {
            break;
        }

        controls_frame_complete(
            &controls
        );

        build_rotated_frame(
            &machine,
            pixels
        );

        if (
            SDL_UpdateTexture(
                texture,
                NULL,
                pixels,
                SOURCE_WIDTH *
                    sizeof(uint16_t)
            ) != 0
        ) {
            fprintf(
                stderr,
                "SDL_UpdateTexture failed: %s\n",
                SDL_GetError()
            );

            break;
        }

        SDL_RenderClear(
            renderer
        );

        if (
            SDL_RenderCopy(
                renderer,
                texture,
                NULL,
                &content_rect
            ) != 0
        ) {
            fprintf(
                stderr,
                "SDL_RenderCopy failed: %s\n",
                SDL_GetError()
            );

            break;
        }

        SDL_RenderPresent(
            renderer
        );

        displayed_frames++;

        if (
            displayed_frames % 60u ==
            0u
        ) {
            char title[256];

            snprintf(
                title,
                sizeof(title),
                "arcade-core | Pac-Man | "
                "224x288 -> 448x576 @ 1280x720 | "
                "frame %llu | PC=%04x | "
                "arrows | 5 coin | 1 start | "
                "F fullscreen | Esc quit",
                (unsigned long long)
                    machine.video_timing.frame,
                machine.cpu_board.cpu.pc
            );

            SDL_SetWindowTitle(
                window,
                title
            );
        }

        const uint64_t frame_end =
            SDL_GetPerformanceCounter();

        const double elapsed =
            (double)(
                frame_end -
                frame_start
            ) /
            (double)performance_frequency;

        if (elapsed < frame_seconds) {
            const double remaining =
                frame_seconds -
                elapsed;

            const uint32_t milliseconds =
                (uint32_t)(
                    remaining *
                    1000.0
                );

            if (milliseconds > 0u) {
                SDL_Delay(
                    milliseconds
                );
            }
        }
    }

    printf(
        "Displayed frames: %llu\n",
        (unsigned long long)
            displayed_frames
    );

    printf(
        "Emulated frames: %llu\n",
        (unsigned long long)
            machine.video_timing.frame
    );

    free(
        pixels
    );

    SDL_DestroyTexture(
        texture
    );

    SDL_DestroyRenderer(
        renderer
    );

    SDL_DestroyWindow(
        window
    );

    SDL_Quit();

    return EXIT_SUCCESS;
}
