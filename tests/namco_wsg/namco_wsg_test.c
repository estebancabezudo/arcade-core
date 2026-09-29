#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "namco_wsg.h"

static void fill_test_prom(
    uint8_t *prom
) {
    for (
        unsigned waveform = 0;
        waveform < NAMCO_WSG_WAVEFORM_COUNT;
        waveform++
    ) {
        for (
            unsigned position = 0;
            position < NAMCO_WSG_WAVEFORM_LENGTH;
            position++
        ) {
            prom[
                waveform *
                    NAMCO_WSG_WAVEFORM_LENGTH +
                position
            ] =
                (uint8_t)(
                    (
                        8u +
                        waveform +
                        position
                    ) &
                    0x0fu
                );
        }
    }
}

static void test_initialization(void)
{
    uint8_t prom[
        NAMCO_WSG_WAVEFORM_PROM_SIZE
    ];

    fill_test_prom(
        prom
    );

    NamcoWsg device;

    assert(
        namco_wsg_init(
            &device,
            prom,
            sizeof(prom)
        )
    );

    assert(device.enabled);
    assert(
        device.waveform_prom ==
        prom
    );

    for (
        unsigned i = 0;
        i < NAMCO_WSG_VOICE_COUNT;
        i++
    ) {
        assert(
            device.voices[i].frequency ==
            0u
        );

        assert(
            device.voices[i].counter ==
            0u
        );

        assert(
            device.voices[i].waveform ==
            0u
        );

        assert(
            device.voices[i].volume ==
            0u
        );
    }

    assert(
        !namco_wsg_init(
            &device,
            prom,
            sizeof(prom) - 1u
        )
    );
}

static void test_register_map(void)
{
    uint8_t prom[
        NAMCO_WSG_WAVEFORM_PROM_SIZE
    ] = {0};

    NamcoWsg device;

    assert(
        namco_wsg_init(
            &device,
            prom,
            sizeof(prom)
        )
    );

    /*
     * High bits must be discarded.
     */
    namco_wsg_write_register(
        &device,
        0x05,
        0xf9
    );

    assert(
        device.registers[0x05] ==
        0x09
    );

    assert(
        device.voices[0].waveform ==
        1u
    );

    namco_wsg_write_register(
        &device,
        0x0a,
        0x06
    );

    namco_wsg_write_register(
        &device,
        0x0f,
        0x07
    );

    assert(
        device.voices[1].waveform ==
        6u
    );

    assert(
        device.voices[2].waveform ==
        7u
    );

    /*
     * Voice 0:
     *
     * 0x10..0x14 = 1,2,3,4,5
     * frequency = 0x54321
     */
    namco_wsg_write_register(
        &device,
        0x10,
        1
    );

    namco_wsg_write_register(
        &device,
        0x11,
        2
    );

    namco_wsg_write_register(
        &device,
        0x12,
        3
    );

    namco_wsg_write_register(
        &device,
        0x13,
        4
    );

    namco_wsg_write_register(
        &device,
        0x14,
        5
    );

    assert(
        device.voices[0].frequency ==
        UINT32_C(0x54321)
    );

    /*
     * Voice 1 has no extra low nibble.
     */
    namco_wsg_write_register(
        &device,
        0x16,
        1
    );

    namco_wsg_write_register(
        &device,
        0x17,
        2
    );

    namco_wsg_write_register(
        &device,
        0x18,
        3
    );

    namco_wsg_write_register(
        &device,
        0x19,
        4
    );

    assert(
        device.voices[1].frequency ==
        UINT32_C(0x43210)
    );

    namco_wsg_write_register(
        &device,
        0x1b,
        5
    );

    namco_wsg_write_register(
        &device,
        0x1c,
        6
    );

    namco_wsg_write_register(
        &device,
        0x1d,
        7
    );

    namco_wsg_write_register(
        &device,
        0x1e,
        8
    );

    assert(
        device.voices[2].frequency ==
        UINT32_C(0x87650)
    );

    namco_wsg_write_register(
        &device,
        0x15,
        0x1f
    );

    namco_wsg_write_register(
        &device,
        0x1a,
        0x2e
    );

    namco_wsg_write_register(
        &device,
        0x1f,
        0x3d
    );

    assert(
        device.voices[0].volume ==
        15u
    );

    assert(
        device.voices[1].volume ==
        14u
    );

    assert(
        device.voices[2].volume ==
        13u
    );
}

static void test_waveform_and_phase(void)
{
    uint8_t prom[
        NAMCO_WSG_WAVEFORM_PROM_SIZE
    ];

    fill_test_prom(
        prom
    );

    NamcoWsg device;

    assert(
        namco_wsg_init(
            &device,
            prom,
            sizeof(prom)
        )
    );

    /*
     * Waveform 0:
     *
     * position 0 -> PROM 8 -> sample 0
     * position 1 -> PROM 9 -> sample +1
     * position 2 -> PROM A -> sample +2
     *
     * frequency 0x10000 advances exactly one waveform
     * position per 192 kHz output sample.
     */
    namco_wsg_write_register(
        &device,
        0x14,
        1
    );

    namco_wsg_write_register(
        &device,
        0x15,
        1
    );

    assert(
        namco_wsg_next_sample(
            &device
        ) ==
        0
    );

    assert(
        device.voices[0].counter ==
        UINT32_C(0x10000)
    );

    assert(
        namco_wsg_next_sample(
            &device
        ) ==
        85
    );

    assert(
        device.voices[0].counter ==
        UINT32_C(0x20000)
    );

    assert(
        namco_wsg_next_sample(
            &device
        ) ==
        170
    );
}

static void test_three_voice_mix_and_enable(void)
{
    uint8_t prom[
        NAMCO_WSG_WAVEFORM_PROM_SIZE
    ];

    memset(
        prom,
        9,
        sizeof(prom)
    );

    NamcoWsg device;

    assert(
        namco_wsg_init(
            &device,
            prom,
            sizeof(prom)
        )
    );

    namco_wsg_write_register(
        &device,
        0x15,
        1
    );

    namco_wsg_write_register(
        &device,
        0x1a,
        1
    );

    namco_wsg_write_register(
        &device,
        0x1f,
        1
    );

    /*
     * Three voices each contribute +1.
     */
    assert(
        namco_wsg_next_sample(
            &device
        ) ==
        255
    );

    /*
     * When globally disabled the output is zero and phase
     * counters remain frozen.
     */
    device.voices[0].frequency =
        UINT32_C(0x10000);

    const uint32_t counter_before =
        device.voices[0].counter;

    namco_wsg_set_enabled(
        &device,
        false
    );

    assert(
        namco_wsg_next_sample(
            &device
        ) ==
        0
    );

    assert(
        device.voices[0].counter ==
        counter_before
    );

    namco_wsg_set_enabled(
        &device,
        true
    );

    assert(
        namco_wsg_next_sample(
            &device
        ) !=
        0
    );
}

int main(void)
{
    test_initialization();
    test_register_map();
    test_waveform_and_phase();
    test_three_voice_mix_and_enable();

    puts(
        "PASS Namco 3-voice WSG device"
    );

    return 0;
}
