#!/usr/bin/env python3
"""Generate build/standalone/native/pacman.c from shared arcade-core sources.

The modular implementation remains the source of truth. This script only reads it.
"""
from pathlib import Path
import argparse
import tempfile

HERE = Path(__file__).resolve()
STANDALONE = HERE.parents[1]
ROOT = STANDALONE.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, default=ROOT / "build/standalone/native/pacman.c")
OUTPUT = parser.parse_args().output.resolve()

HEADERS = [
    "common/devices/z80/z80.h",
    "common/devices/namco_wsg/namco_wsg.h",
    "common/boards/pacman_cpu/pacman_cpu_board.h",
    "common/boards/pacman_video/pacman_video_board.h",
    "common/boards/pacman_audio/pacman_audio_board.h",
    "common/machines/pacman/pacman_machine.h",
]

SOURCES = [
    "common/devices/z80/z80.c",
    "common/devices/namco_wsg/namco_wsg.c",
    "common/boards/pacman_cpu/pacman_cpu_board.c",
    "common/boards/pacman_video/pacman_video_board.c",
    "common/boards/pacman_video/pacman_video_timing.c",
    "common/boards/pacman_audio/pacman_audio_board.c",
    "common/machines/pacman/pacman_machine.c",
]


def require_files(paths):
    missing = [p for p in paths if not (ROOT / p).is_file()]
    if missing:
        raise SystemExit("Missing arcade-core source files:\n  " + "\n  ".join(missing))


def strip_header(text: str) -> str:
    lines = text.splitlines()
    out = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("#include"):
            continue
        if stripped.startswith("#ifndef ARCADE_CORE_"):
            continue
        if stripped.startswith("#define ARCADE_CORE_"):
            continue
        if stripped == "#endif":
            continue
        out.append(line)
    return "\n".join(out).strip() + "\n"


def strip_source(text: str) -> str:
    lines = []
    for line in text.splitlines():
        if line.strip().startswith("#include"):
            continue
        lines.append(line)
    return "\n".join(lines).strip() + "\n"


require_files(HEADERS + SOURCES)

parts = []
parts.append(r'''/*
 * GENERATED FILE - DO NOT EDIT BY HAND.
 *
 * Standalone Pac-Man amalgamation generated from arcade-core's validated
 * modular Z80, Namco WSG, Pac-Man board and PacmanMachine implementations.
 *
 * Regenerate with:
 *   python3 standalone/tools/generate_pacman.py
 *
 * The standalone layer deliberately has no dependency on SDL, filesystems,
 * malloc, an operating system, or the RTL/CRT video pipeline.
 */

#include "pacman.h"

/* Minimal freestanding replacements used by the original modular sources. */
static void *pacman_memset(void *destination, int value, size_t count)
{
    unsigned char *out = (unsigned char *)destination;
    for (size_t i = 0; i < count; i++) {
        out[i] = (unsigned char)value;
    }
    return destination;
}

#define memset pacman_memset
''')

for path in HEADERS:
    parts.append(f"\n/* ===== BEGIN {path} ===== */\n")
    parts.append(strip_header((ROOT / path).read_text(encoding="utf-8")))
    parts.append(f"/* ===== END {path} ===== */\n")

for path in SOURCES:
    parts.append(f"\n/* ===== BEGIN {path} ===== */\n")
    parts.append(strip_source((ROOT / path).read_text(encoding="utf-8")))
    parts.append(f"/* ===== END {path} ===== */\n")

parts.append(r'''
#undef memset

/* ===== Standalone wrapper ===== */

enum {
    PACMAN_STANDALONE_AUDIO_DECIMATION =
        NAMCO_WSG_INTERNAL_SAMPLE_RATE /
        PACMAN_STANDALONE_AUDIO_RATE,
    PACMAN_STANDALONE_AUDIO_BUFFER_SAMPLES = 1024,
    PACMAN_STANDALONE_FRAME_US = 16500
};

_Static_assert(
    NAMCO_WSG_INTERNAL_SAMPLE_RATE % PACMAN_STANDALONE_AUDIO_RATE == 0,
    "Standalone audio rate must divide the WSG rate exactly"
);

_Static_assert(
    PACMAN_STANDALONE_AUDIO_DECIMATION == 4,
    "Pac-Man standalone expects 192 kHz -> 48 kHz decimation"
);

static PacmanMachine standalone_machine;
static bool standalone_initialized;

static uint16_t standalone_framebuffer[
    PACMAN_STANDALONE_WIDTH * PACMAN_STANDALONE_HEIGHT
];

static int16_t standalone_audio_buffer[
    PACMAN_STANDALONE_AUDIO_BUFFER_SAMPLES
];

static size_t standalone_audio_samples;
static int32_t standalone_audio_sum;
static unsigned standalone_audio_decimation_count;
static bool standalone_audio_overflow;

static bool standalone_previous_coin;
static unsigned standalone_coin_frames;
static uint8_t standalone_last_direction;

static bool standalone_previous_up;
static bool standalone_previous_down;
static bool standalone_previous_left;
static bool standalone_previous_right;

static uint16_t standalone_rgb565(PacmanRgb color)
{
    return (uint16_t)(
        ((uint16_t)(color.r >> 3) << 11) |
        ((uint16_t)(color.g >> 2) << 5) |
        ((uint16_t)(color.b >> 3) << 0)
    );
}

static void standalone_audio_sample(void *context, int16_t sample)
{
    (void)context;

    standalone_audio_sum += sample;
    standalone_audio_decimation_count++;

    if (
        standalone_audio_decimation_count <
        PACMAN_STANDALONE_AUDIO_DECIMATION
    ) {
        return;
    }

    if (
        standalone_audio_samples >=
        PACMAN_STANDALONE_AUDIO_BUFFER_SAMPLES
    ) {
        standalone_audio_overflow = true;
        standalone_audio_sum = 0;
        standalone_audio_decimation_count = 0;
        return;
    }

    standalone_audio_buffer[standalone_audio_samples++] =
        (int16_t)(
            standalone_audio_sum /
            PACMAN_STANDALONE_AUDIO_DECIMATION
        );

    standalone_audio_sum = 0;
    standalone_audio_decimation_count = 0;
}

static bool standalone_direction_held(
    PacmanStandaloneInput input,
    uint8_t direction
)
{
    switch (direction) {
        case 0x01u:
            return input.up;
        case 0x02u:
            return input.left;
        case 0x04u:
            return input.right;
        case 0x08u:
            return input.down;
        default:
            return false;
    }
}

static uint8_t standalone_select_direction(PacmanStandaloneInput input)
{
    if (input.up && !standalone_previous_up) {
        standalone_last_direction = 0x01u;
    }
    if (input.left && !standalone_previous_left) {
        standalone_last_direction = 0x02u;
    }
    if (input.right && !standalone_previous_right) {
        standalone_last_direction = 0x04u;
    }
    if (input.down && !standalone_previous_down) {
        standalone_last_direction = 0x08u;
    }

    standalone_previous_up = input.up;
    standalone_previous_down = input.down;
    standalone_previous_left = input.left;
    standalone_previous_right = input.right;

    if (standalone_direction_held(input, standalone_last_direction)) {
        return standalone_last_direction;
    }

    if (input.up) {
        standalone_last_direction = 0x01u;
    } else if (input.left) {
        standalone_last_direction = 0x02u;
    } else if (input.right) {
        standalone_last_direction = 0x04u;
    } else if (input.down) {
        standalone_last_direction = 0x08u;
    } else {
        standalone_last_direction = 0u;
    }

    return standalone_last_direction;
}

static void standalone_apply_input(PacmanStandaloneInput input)
{
    uint8_t input0 = 0xffu;
    uint8_t input1 = 0xffu;

    input0 &= (uint8_t)~standalone_select_direction(input);

    if (input.coin && !standalone_previous_coin) {
        standalone_coin_frames = 2u;
    }
    standalone_previous_coin = input.coin;

    if (standalone_coin_frames != 0u) {
        input0 &= (uint8_t)~0x20u;
    }

    if (input.start1) {
        input1 &= (uint8_t)~0x20u;
    }

    if (input.start2) {
        input1 &= (uint8_t)~0x40u;
    }

    standalone_machine.input0 = input0;
    standalone_machine.input1 = input1;
}

static void standalone_input_frame_complete(void)
{
    if (standalone_coin_frames != 0u) {
        standalone_coin_frames--;
    }
}

static bool standalone_run_machine_frame(void)
{
    const uint64_t target_frame =
        standalone_machine.video_timing.frame + 1u;

    while (standalone_machine.video_timing.frame < target_frame) {
        if (pacman_machine_step(&standalone_machine) == 0u) {
            return false;
        }
    }

    return true;
}

static void standalone_render_frame(void)
{
    for (uint16_t y = 0; y < PACMAN_VISIBLE_HEIGHT; y++) {
        for (uint16_t x = 0; x < PACMAN_VISIBLE_WIDTH; x++) {
            const PacmanRgb color =
                pacman_machine_video_pixel(
                    &standalone_machine,
                    x,
                    y
                );

            const uint16_t display_x =
                (uint16_t)(PACMAN_VISIBLE_HEIGHT - 1u - y);
            const uint16_t display_y = x;

            standalone_framebuffer[
                (size_t)display_y * PACMAN_STANDALONE_WIDTH +
                display_x
            ] = standalone_rgb565(color);
        }
    }
}

bool pacman_standalone_init(const PacmanStandaloneRomSet *roms)
{
    if (
        roms == NULL ||
        roms->program == NULL ||
        roms->program_size != PACMAN_STANDALONE_PROGRAM_ROM_SIZE ||
        roms->tile == NULL ||
        roms->tile_size != PACMAN_STANDALONE_TILE_ROM_SIZE ||
        roms->sprite == NULL ||
        roms->sprite_size != PACMAN_STANDALONE_SPRITE_ROM_SIZE ||
        roms->palette == NULL ||
        roms->palette_size != PACMAN_STANDALONE_PALETTE_PROM_SIZE ||
        roms->color_lookup == NULL ||
        roms->color_lookup_size != PACMAN_STANDALONE_COLOR_LOOKUP_PROM_SIZE ||
        roms->sound_waveform == NULL ||
        roms->sound_waveform_size !=
            PACMAN_STANDALONE_SOUND_WAVEFORM_PROM_SIZE ||
        roms->sound_timing == NULL ||
        roms->sound_timing_size !=
            PACMAN_STANDALONE_SOUND_TIMING_PROM_SIZE
    ) {
        return false;
    }

    if (
        !pacman_machine_init(
            &standalone_machine,
            roms->program,
            roms->program_size,
            roms->tile,
            roms->tile_size,
            roms->sprite,
            roms->sprite_size,
            roms->palette,
            roms->palette_size,
            roms->color_lookup,
            roms->color_lookup_size,
            roms->sound_waveform,
            roms->sound_waveform_size,
            roms->sound_timing,
            roms->sound_timing_size
        )
    ) {
        return false;
    }

    standalone_machine.dsw1 = 0xc9u;

    pacman_machine_set_audio_sink(
        &standalone_machine,
        standalone_audio_sample,
        NULL
    );

    standalone_audio_samples = 0u;
    standalone_audio_sum = 0;
    standalone_audio_decimation_count = 0u;
    standalone_audio_overflow = false;
    standalone_previous_coin = false;
    standalone_coin_frames = 0u;
    standalone_last_direction = 0u;
    standalone_previous_up = false;
    standalone_previous_down = false;
    standalone_previous_left = false;
    standalone_previous_right = false;
    standalone_initialized = true;

    return true;
}

void pacman_standalone_reset(void)
{
    if (!standalone_initialized) {
        return;
    }

    pacman_machine_reset(&standalone_machine);
    standalone_machine.dsw1 = 0xc9u;

    standalone_audio_samples = 0u;
    standalone_audio_sum = 0;
    standalone_audio_decimation_count = 0u;
    standalone_audio_overflow = false;
    standalone_previous_coin = false;
    standalone_coin_frames = 0u;
    standalone_last_direction = 0u;
    standalone_previous_up = false;
    standalone_previous_down = false;
    standalone_previous_left = false;
    standalone_previous_right = false;
}

bool pacman_standalone_frame(PacmanStandaloneInput input)
{
    if (!standalone_initialized) {
        return false;
    }

    standalone_audio_samples = 0u;
    standalone_audio_overflow = false;

    standalone_apply_input(input);

    if (!standalone_run_machine_frame()) {
        return false;
    }

    standalone_input_frame_complete();

    if (standalone_audio_overflow) {
        return false;
    }

    standalone_render_frame();

    platform_video_present(
        standalone_framebuffer,
        PACMAN_STANDALONE_WIDTH * PACMAN_STANDALONE_HEIGHT
    );

    if (standalone_audio_samples != 0u) {
        platform_audio_write(
            standalone_audio_buffer,
            standalone_audio_samples
        );
    }

    return true;
}

void pacman_standalone_run(void)
{
    if (!standalone_initialized) {
        return;
    }

    uint64_t deadline = platform_time_us();

    for (;;) {
        const PacmanStandaloneInput input = platform_input_read();

        if (!pacman_standalone_frame(input)) {
            return;
        }

        deadline += PACMAN_STANDALONE_FRAME_US;

        while (platform_time_us() < deadline) {
            platform_idle();
        }

        const uint64_t now = platform_time_us();
        if (now > deadline + PACMAN_STANDALONE_FRAME_US) {
            deadline = now;
        }
    }
}
''')

content = "".join(parts).encode("utf-8")
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
# Keep timestamps stable when inputs are unchanged, so Make can stay incremental.
if not OUTPUT.exists() or OUTPUT.read_bytes() != content:
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=OUTPUT.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(content)
        temporary.chmod(0o644)
        temporary.replace(OUTPUT)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
print(f"generated={OUTPUT}")
print(f"bytes={OUTPUT.stat().st_size}")
