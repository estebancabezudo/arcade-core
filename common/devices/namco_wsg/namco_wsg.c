#include "namco_wsg.h"

#include <string.h>

static void recompute_frequency(
    NamcoWsg *device,
    unsigned voice_index
) {
    NamcoWsgVoice *voice =
        &device->voices[voice_index];

    switch (voice_index) {
        case 0:
            /*
             * Voice 0 has one extra low-frequency nibble.
             *
             * 10,11,12,13,14 form a 20-bit value.
             */
            voice->frequency =
                ((uint32_t)device->registers[0x10] << 0) |
                ((uint32_t)device->registers[0x11] << 4) |
                ((uint32_t)device->registers[0x12] << 8) |
                ((uint32_t)device->registers[0x13] << 12) |
                ((uint32_t)device->registers[0x14] << 16);
            break;

        case 1:
            /*
             * Voices 1 and 2 do not have the extra low nibble,
             * therefore their four programmed nibbles begin at
             * frequency bit 4.
             */
            voice->frequency =
                ((uint32_t)device->registers[0x16] << 4) |
                ((uint32_t)device->registers[0x17] << 8) |
                ((uint32_t)device->registers[0x18] << 12) |
                ((uint32_t)device->registers[0x19] << 16);
            break;

        case 2:
            voice->frequency =
                ((uint32_t)device->registers[0x1b] << 4) |
                ((uint32_t)device->registers[0x1c] << 8) |
                ((uint32_t)device->registers[0x1d] << 12) |
                ((uint32_t)device->registers[0x1e] << 16);
            break;

        default:
            break;
    }
}

bool namco_wsg_init(
    NamcoWsg *device,
    const uint8_t *waveform_prom,
    size_t waveform_prom_size
) {
    if (
        device == NULL ||
        waveform_prom == NULL ||
        waveform_prom_size !=
            NAMCO_WSG_WAVEFORM_PROM_SIZE
    ) {
        return false;
    }

    memset(
        device,
        0,
        sizeof(*device)
    );

    device->waveform_prom =
        waveform_prom;

    device->waveform_prom_size =
        waveform_prom_size;

    namco_wsg_reset(
        device
    );

    return true;
}

void namco_wsg_reset(
    NamcoWsg *device
) {
    if (device == NULL) {
        return;
    }

    memset(
        device->registers,
        0,
        sizeof(device->registers)
    );

    memset(
        device->voices,
        0,
        sizeof(device->voices)
    );

    /*
     * The generic WSG device starts enabled.
     *
     * Pac-Man's external sound-enable latch will explicitly
     * disable it when the machine is reset.
     */
    device->enabled = true;
}

void namco_wsg_set_enabled(
    NamcoWsg *device,
    bool enabled
) {
    if (device == NULL) {
        return;
    }

    device->enabled =
        enabled;
}

void namco_wsg_write_register(
    NamcoWsg *device,
    uint16_t offset,
    uint8_t value
) {
    if (
        device == NULL ||
        offset >= NAMCO_WSG_REGISTER_COUNT
    ) {
        return;
    }

    /*
     * The physical sound registers consume only the low nibble.
     */
    value &= 0x0fu;

    device->registers[offset] =
        value;

    switch (offset) {
        /*
         * Waveform select.
         */
        case 0x05:
            device->voices[0].waveform =
                value & 0x07u;
            return;

        case 0x0a:
            device->voices[1].waveform =
                value & 0x07u;
            return;

        case 0x0f:
            device->voices[2].waveform =
                value & 0x07u;
            return;

        /*
         * Voice 0 frequency.
         */
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
            recompute_frequency(
                device,
                0
            );
            return;

        case 0x15:
            device->voices[0].volume =
                value;
            return;

        /*
         * Voice 1 frequency.
         */
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
            recompute_frequency(
                device,
                1
            );
            return;

        case 0x1a:
            device->voices[1].volume =
                value;
            return;

        /*
         * Voice 2 frequency.
         */
        case 0x1b:
        case 0x1c:
        case 0x1d:
        case 0x1e:
            recompute_frequency(
                device,
                2
            );
            return;

        case 0x1f:
            device->voices[2].volume =
                value;
            return;

        /*
         * 00-04, 06-09 and 0B-0E are accumulator-related
         * register positions. Software-visible writes are retained,
         * but they do not directly alter the synthesized parameters.
         */
        default:
            return;
    }
}

static int waveform_sample(
    const NamcoWsg *device,
    const NamcoWsgVoice *voice
) {
    const uint32_t position =
        (
            voice->counter >>
            NAMCO_WSG_PHASE_FRACTION_BITS
        ) &
        (NAMCO_WSG_WAVEFORM_LENGTH - 1u);

    const uint32_t address =
        (uint32_t)voice->waveform *
            NAMCO_WSG_WAVEFORM_LENGTH +
        position;

    /*
     * Only the PROM's low nibble is connected to the waveform DAC.
     *
     * 0..15 becomes signed -8..7 around the analogue midpoint.
     */
    return
        (int)(
            device->waveform_prom[address] &
            0x0fu
        ) -
        8;
}

int16_t namco_wsg_next_sample(
    NamcoWsg *device
) {
    if (
        device == NULL ||
        !device->enabled
    ) {
        return 0;
    }

    int32_t mix = 0;

    for (
        unsigned i = 0;
        i < NAMCO_WSG_VOICE_COUNT;
        i++
    ) {
        NamcoWsgVoice *voice =
            &device->voices[i];

        /*
         * Match the hardware/reference stream behavior:
         * a silent voice does not advance its phase counter.
         */
        if (voice->volume == 0u) {
            continue;
        }

        mix +=
            waveform_sample(
                device,
                voice
            ) *
            (int32_t)voice->volume;

        voice->counter +=
            voice->frequency;
    }

    /*
     * Convert the normalized three-voice mix to PCM16.
     *
     * Maximum theoretical raw magnitude remains below int16_t
     * after this normalization, so no clipping stage is required.
     */
    const int32_t pcm =
        (
            mix *
            INT32_C(32767)
        ) /
        NAMCO_WSG_MIX_RESOLUTION;

    return (int16_t)pcm;
}
