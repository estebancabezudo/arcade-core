#ifndef ARCADE_CORE_NAMCO_WSG_H
#define ARCADE_CORE_NAMCO_WSG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    NAMCO_WSG_VOICE_COUNT = 3,
    NAMCO_WSG_REGISTER_COUNT = 0x20,

    NAMCO_WSG_WAVEFORM_COUNT = 8,
    NAMCO_WSG_WAVEFORM_LENGTH = 32,
    NAMCO_WSG_WAVEFORM_PROM_SIZE =
        NAMCO_WSG_WAVEFORM_COUNT *
        NAMCO_WSG_WAVEFORM_LENGTH,

    /*
     * Pac-Man's WSG is modeled internally at 192 kHz.
     *
     * The 32-sample waveform position is selected from
     * counter bits above a 16-bit fractional phase.
     */
    NAMCO_WSG_INTERNAL_SAMPLE_RATE = 192000,
    NAMCO_WSG_PHASE_FRACTION_BITS = 16,

    /*
     * Three voices, each with waveform range -8..7 and
     * volume 0..15.
     *
     * This matches the normalization used by the reference
     * Namco WSG implementation.
     */
    NAMCO_WSG_MIX_RESOLUTION =
        128 * NAMCO_WSG_VOICE_COUNT
};

typedef struct {
    uint32_t frequency;
    uint32_t counter;

    uint8_t waveform;
    uint8_t volume;
} NamcoWsgVoice;

typedef struct {
    const uint8_t *waveform_prom;
    size_t waveform_prom_size;

    uint8_t registers[NAMCO_WSG_REGISTER_COUNT];

    NamcoWsgVoice voices[NAMCO_WSG_VOICE_COUNT];

    bool enabled;
} NamcoWsg;

bool namco_wsg_init(
    NamcoWsg *device,
    const uint8_t *waveform_prom,
    size_t waveform_prom_size
);

void namco_wsg_reset(
    NamcoWsg *device
);

void namco_wsg_set_enabled(
    NamcoWsg *device,
    bool enabled
);

void namco_wsg_write_register(
    NamcoWsg *device,
    uint16_t offset,
    uint8_t value
);

/*
 * Generate one signed 16-bit mono sample at
 * NAMCO_WSG_INTERNAL_SAMPLE_RATE.
 */
int16_t namco_wsg_next_sample(
    NamcoWsg *device
);

#endif
