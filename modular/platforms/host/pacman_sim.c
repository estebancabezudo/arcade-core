#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "pacman_machine.h"
#include "pacman_rom_loader.h"

enum {
    DISPLAY_WIDTH = PACMAN_VISIBLE_HEIGHT,
    DISPLAY_HEIGHT = PACMAN_VISIBLE_WIDTH,

    WINDOW_SCALE = 3,

    HOST_AUDIO_RATE = 48000,
    HOST_AUDIO_DECIMATION = 4,
    HOST_AUDIO_BUFFER_SAMPLES = 512
};

_Static_assert(
    NAMCO_WSG_INTERNAL_SAMPLE_RATE /
        HOST_AUDIO_DECIMATION ==
        HOST_AUDIO_RATE,
    "Host audio rate must divide WSG rate exactly"
);

_Static_assert(
    NAMCO_WSG_INTERNAL_SAMPLE_RATE %
        HOST_AUDIO_DECIMATION ==
        0,
    "WSG rate must be an integer multiple of host audio rate"
);

typedef struct {
    SDL_AudioDeviceID device;
    int32_t decimation_sum;
    unsigned decimation_count;

    int16_t buffer[
        HOST_AUDIO_BUFFER_SAMPLES
    ];

    size_t buffered_samples;

    bool started;
    bool failed;
} HostAudio;


static void host_audio_flush(
    HostAudio *audio
) {
    if (
        audio == NULL ||
        audio->failed ||
        audio->buffered_samples == 0u
    ) {
        return;
    }

    if (
        SDL_QueueAudio(
            audio->device,
            audio->buffer,
            (uint32_t)(
                audio->buffered_samples *
                sizeof(audio->buffer[0])
            )
        ) != 0
    ) {
        audio->failed = true;
        return;
    }

    audio->buffered_samples = 0u;
}


static void host_audio_sample(
    void *context,
    int16_t sample
) {
    HostAudio *audio = context;

    if (
        audio == NULL ||
        audio->failed
    ) {
        return;
    }

    audio->decimation_sum +=
        sample;

    audio->decimation_count++;

    if (
        audio->decimation_count <
        HOST_AUDIO_DECIMATION
    ) {
        return;
    }

    const int32_t averaged =
        audio->decimation_sum /
        HOST_AUDIO_DECIMATION;

    audio->decimation_sum = 0;
    audio->decimation_count = 0u;

    audio->buffer[
        audio->buffered_samples++
    ] =
        (int16_t)averaged;

    if (
        audio->buffered_samples ==
        HOST_AUDIO_BUFFER_SAMPLES
    ) {
        host_audio_flush(
            audio
        );
    }
}


static void host_audio_frame_complete(
    HostAudio *audio
) {
    if (
        audio == NULL ||
        audio->failed
    ) {
        return;
    }

    /*
     * A Pac-Man frame produces about 792 samples at 48 kHz.
     * Never leave a partial block waiting for the next frame.
     */
    host_audio_flush(
        audio
    );

    if (!audio->started) {
        const uint32_t queued_bytes =
            SDL_GetQueuedAudioSize(
                audio->device
            );

        const uint32_t start_bytes =
            (uint32_t)(
                (HOST_AUDIO_RATE / 30) *
                sizeof(int16_t)
            );

        if (queued_bytes >= start_bytes) {
            SDL_PauseAudioDevice(
                audio->device,
                0
            );

            audio->started = true;
        }
    }
}


static bool host_audio_open(
    HostAudio *audio
) {
    if (audio == NULL) {
        return false;
    }

    *audio = (HostAudio){0};

    SDL_AudioSpec desired = {0};

    desired.freq =
        HOST_AUDIO_RATE;

    desired.format =
        AUDIO_S16SYS;

    desired.channels = 1;
    desired.samples = 1024;
    desired.callback = NULL;

    audio->device =
        SDL_OpenAudioDevice(
            NULL,
            0,
            &desired,
            NULL,
            0
        );

    if (audio->device == 0) {
        fprintf(
            stderr,
            "SDL_OpenAudioDevice failed: %s\n",
            SDL_GetError()
        );

        return false;
    }

    /*
     * SDL devices start paused.
     *
     * Leave playback paused until roughly two video frames of
     * audio have been queued. This prevents startup and
     * per-frame underruns.
     */
    audio->started = false;

    return true;
}


static bool render_frame(
    SDL_Renderer *renderer,
    SDL_Texture *texture,
    PacmanMachine *machine,
    uint32_t *pixels
) {
    /*
     * Pac-Man's monitor is mounted vertically.
     *
     * Native board raster:
     *
     *   288 x 224
     *
     * Displayed cabinet orientation:
     *
     *   224 x 288
     *
     * Rotate clockwise here. This is outside the emulated
     * Pac-Man video board itself.
     */
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
                display_y *
                    DISPLAY_WIDTH +
                display_x
            ] =
                0xff000000u |
                ((uint32_t)color.r << 16) |
                ((uint32_t)color.g << 8) |
                (uint32_t)color.b;
        }
    }

    if (
        SDL_UpdateTexture(
            texture,
            NULL,
            pixels,
            DISPLAY_WIDTH *
                sizeof(uint32_t)
        ) != 0
    ) {
        fprintf(
            stderr,
            "SDL_UpdateTexture failed: %s\n",
            SDL_GetError()
        );

        return false;
    }

    if (
        SDL_RenderClear(
            renderer
        ) != 0
    ) {
        fprintf(
            stderr,
            "SDL_RenderClear failed: %s\n",
            SDL_GetError()
        );

        return false;
    }

    if (
        SDL_RenderCopy(
            renderer,
            texture,
            NULL,
            NULL
        ) != 0
    ) {
        fprintf(
            stderr,
            "SDL_RenderCopy failed: %s\n",
            SDL_GetError()
        );

        return false;
    }

    SDL_RenderPresent(
        renderer
    );

    return true;
}

typedef struct {
    SDL_Scancode last_direction;

    uint32_t coin_frames;

    bool start1;
    bool start2;
} HostControls;

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
        SDL_GetKeyboardState(NULL);

    /*
     * A real Pac-Man joystick has a 4-way restrictor.
     *
     * Prefer the most recently pressed direction while it
     * remains held, so keyboard diagonals never reach the
     * emulated cabinet.
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

    /*
     * Cabinet inputs are active-low.
     */
    const uint8_t direction =
        current_direction(
            controls
        );

    input0 &=
        (uint8_t)~direction;

    /*
     * Coin 1.
     *
     * Keep the input active for two complete video frames.
     */
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
            switch (event.key.keysym.scancode) {
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
            : "common/roms/pacman";

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

    /*
     * Standard upright Pac-Man settings:
     *
     * 1 coin / 1 credit
     * 3 lives
     * 10000 bonus
     * normal difficulty
     */
    machine.dsw1 = 0xc9u;

    if (
        SDL_Init(
            SDL_INIT_VIDEO |
            SDL_INIT_AUDIO |
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
            "arcade-core | Pac-Man",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,

            DISPLAY_WIDTH *
                WINDOW_SCALE,

            DISPLAY_HEIGHT *
                WINDOW_SCALE,

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
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT
    );

    SDL_RenderSetIntegerScale(
        renderer,
        SDL_TRUE
    );

    SDL_Texture *texture =
        SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT
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

    uint32_t *pixels =
        malloc(
            sizeof(uint32_t) *
            DISPLAY_WIDTH *
            DISPLAY_HEIGHT
        );

    if (pixels == NULL) {
        fprintf(
            stderr,
            "Could not allocate display buffer.\n"
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

    HostAudio audio;

    if (
        !host_audio_open(
            &audio
        )
    ) {
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

        return EXIT_FAILURE;
    }

    pacman_machine_set_audio_sink(
        &machine,
        host_audio_sample,
        &audio
    );

    /*
     * Host-only display buffer allocation is fine.
     *
     * The emulated machine itself performs no dynamic allocation.
     */
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

    const uint64_t frame_ticks =
        (uint64_t)(
            frame_seconds *
            (double)performance_frequency +
            0.5
        );

    uint64_t next_frame_deadline =
        SDL_GetPerformanceCounter();

    while (running) {

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
            running = false;
            break;
        }

        host_audio_frame_complete(
            &audio
        );

        if (audio.failed) {
            fprintf(
                stderr,
                "SDL_QueueAudio failed: %s\n",
                SDL_GetError()
            );

            running = false;
            break;
        }

        controls_frame_complete(
            &controls
        );

        if (
            !render_frame(
                renderer,
                texture,
                &machine,
                pixels
            )
        ) {
            running = false;
            break;
        }

        displayed_frames++;

        if (
            (displayed_frames % 60u) ==
            0u
        ) {
            char title[256];

            snprintf(
                title,
                sizeof(title),
                "arcade-core | Pac-Man | "
                "frame %llu | PC=%04x | "
                "Arrows move | 5 coin | 1 start | "
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

        next_frame_deadline +=
            frame_ticks;

        uint64_t now =
            SDL_GetPerformanceCounter();

        if (now < next_frame_deadline) {
            const uint64_t remaining_ticks =
                next_frame_deadline -
                now;

            const uint32_t remaining_ms =
                (uint32_t)(
                    (
                        remaining_ticks *
                        UINT64_C(1000)
                    ) /
                    performance_frequency
                );

            /*
             * Sleep coarsely, then wait out the final fraction
             * using the high-resolution performance counter.
             */
            if (remaining_ms > 1u) {
                SDL_Delay(
                    remaining_ms - 1u
                );
            }

            while (
                SDL_GetPerformanceCounter() <
                next_frame_deadline
            ) {
            }
        } else {
            /*
             * If the host falls behind, resynchronize instead
             * of accumulating lateness forever.
             */
            next_frame_deadline =
                now;
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

    printf(
        "CPU T-states: %llu\n",
        (unsigned long long)
            machine.cpu_board.cpu.total_t_states
    );

    pacman_machine_set_audio_sink(
        &machine,
        NULL,
        NULL
    );

    SDL_ClearQueuedAudio(
        audio.device
    );

    SDL_CloseAudioDevice(
        audio.device
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
