#include "pacman.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

extern const PacmanStandaloneRomSet raspberry_pacman_roms;

static uint64_t video_hash;
static uint64_t audio_hash;
static size_t frames;
static size_t samples;
static bool nonzero_audio;

static uint64_t hash_word(uint64_t hash, uint16_t value)
{
    hash = (hash ^ (value & 255u)) * UINT64_C(1099511628211);
    return (hash ^ (value >> 8)) * UINT64_C(1099511628211);
}

bool platform_init(void) { return true; }
PacmanStandaloneInput platform_input_read(void)
{
    return (PacmanStandaloneInput){0};
}
uint64_t platform_time_us(void) { return 0; }
void platform_idle(void) {}

void platform_video_present(const uint16_t *pixels, size_t pixel_count)
{
    assert(pixels != NULL);
    assert(pixel_count == PACMAN_STANDALONE_WIDTH * PACMAN_STANDALONE_HEIGHT);
    for (size_t i = 0; i < pixel_count; ++i) {
        video_hash = hash_word(video_hash, pixels[i]);
    }
    ++frames;
}

void platform_audio_write(const int16_t *pcm, size_t sample_count)
{
    assert(pcm != NULL);
    assert(sample_count > 0 && sample_count <= 1024);
    for (size_t i = 0; i < sample_count; ++i) {
        audio_hash = hash_word(audio_hash, (uint16_t)pcm[i]);
        nonzero_audio = nonzero_audio || pcm[i] != 0;
    }
    samples += sample_count;
}

static void run_trace(void)
{
    video_hash = audio_hash = UINT64_C(14695981039346656037);
    frames = samples = 0;
    nonzero_audio = false;
    for (size_t frame = 0; frame < 720; ++frame) {
        PacmanStandaloneInput input = {0};
        input.coin = frame == 360;
        input.start1 = frame >= 378 && frame < 382;
        input.left = frame >= 450 && frame < 650;
        input.up = frame >= 700 && frame < 750;
        assert(pacman_standalone_frame(input));
    }
    assert(frames == 720);
    assert(samples == 570240);
    assert(nonzero_audio);
}

int main(void)
{
    pacman_standalone_reset();
    assert(!pacman_standalone_frame((PacmanStandaloneInput){0}));
    assert(!pacman_standalone_init(NULL));
    PacmanStandaloneRomSet invalid = raspberry_pacman_roms;
    invalid.program_size = 0;
    assert(!pacman_standalone_init(&invalid));
    assert(pacman_standalone_init(&raspberry_pacman_roms));
    run_trace();
    printf("cold frames=%zu samples=%zu video=%016" PRIx64 " audio=%016" PRIx64 "\n",
           frames, samples, video_hash, audio_hash);
    /* Hardware reset intentionally retains SRAM; compare its own reference. */
    pacman_standalone_reset();
    run_trace();
    printf("reset frames=%zu samples=%zu video=%016" PRIx64 " audio=%016" PRIx64 "\n",
           frames, samples, video_hash, audio_hash);
    return 0;
}
