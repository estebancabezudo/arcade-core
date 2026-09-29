#ifndef ARCADE_CORE_PACMAN_MACHINE_H
#define ARCADE_CORE_PACMAN_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pacman_audio_board.h"
#include "pacman_cpu_board.h"
#include "pacman_video_board.h"


typedef void (*PacmanMachineAudioSampleCallback)(
    void *context,
    int16_t sample
);


typedef struct {
    PacmanCpuBoard cpu_board;
    PacmanVideoBoard video_board;
    PacmanAudioBoard audio_board;

    /*
     * Original Pac-Man raster clock state.
     */
    PacmanVideoTiming video_timing;

    /*
     * Cabinet/input state.
     *
     * Inputs are active-low on the original hardware.
     */
    uint8_t input0;
    uint8_t input1;
    uint8_t dsw1;
    uint8_t dsw2;

    uint32_t watchdog_kicks;
    bool coin_counter;

    /*
     * 3.072 MHz CPU / 192 kHz WSG = 16 T-states per sample.
     */
    uint32_t audio_t_state_accumulator;

    /*
     * Sound-register writes performed by the current Z80
     * instruction are committed after audio for that
     * instruction's elapsed T-states has been generated.
     */
    uint8_t pending_sound_registers[
        PACMAN_SOUND_REGISTER_COUNT
    ];

    uint32_t pending_sound_register_mask;

    void *audio_sample_context;
    PacmanMachineAudioSampleCallback audio_sample_callback;
} PacmanMachine;

bool pacman_machine_init(
    PacmanMachine *machine,

    const uint8_t *program_rom,
    size_t program_rom_size,

    const uint8_t *tile_rom,
    size_t tile_rom_size,

    const uint8_t *sprite_rom,
    size_t sprite_rom_size,

    const uint8_t *palette_prom,
    size_t palette_prom_size,

    const uint8_t *color_lookup_prom,
    size_t color_lookup_prom_size,

    const uint8_t *sound_waveform_prom,
    size_t sound_waveform_prom_size,

    const uint8_t *sound_timing_prom,
    size_t sound_timing_prom_size
);

void pacman_machine_reset(
    PacmanMachine *machine
);

void pacman_machine_set_audio_sink(
    PacmanMachine *machine,
    PacmanMachineAudioSampleCallback callback,
    void *context
);

/*
 * Execute one complete Z80 instruction or interrupt.
 *
 * Video timing advances automatically according to the
 * T-states consumed by the CPU.
 */
uint32_t pacman_machine_step(
    PacmanMachine *machine
);

uint64_t pacman_machine_run(
    PacmanMachine *machine,
    uint64_t t_state_budget
);

/*
 * Primarily exposed for board-level testing.
 *
 * Normal machine execution drives VBlank automatically
 * from video_timing.
 */
void pacman_machine_set_vblank(
    PacmanMachine *machine,
    bool active
);

PacmanRgb pacman_machine_video_pixel(
    const PacmanMachine *machine,
    uint16_t x,
    uint16_t y
);

#endif
