#include "pacman_machine.h"

#include <string.h>


enum {
    PACMAN_AUDIO_T_STATES_PER_SAMPLE =
        PACMAN_CPU_CLOCK_HZ /
        NAMCO_WSG_INTERNAL_SAMPLE_RATE
};

_Static_assert(
    PACMAN_CPU_CLOCK_HZ %
        NAMCO_WSG_INTERNAL_SAMPLE_RATE ==
        0,
    "Pac-Man CPU clock must divide exactly into WSG sample rate"
);

static uint8_t read_input0(
    void *context
) {
    PacmanMachine *machine = context;
    return machine->input0;
}

static uint8_t read_input1(
    void *context
) {
    PacmanMachine *machine = context;
    return machine->input1;
}

static uint8_t read_dsw1(
    void *context
) {
    PacmanMachine *machine = context;
    return machine->dsw1;
}

static uint8_t read_dsw2(
    void *context
) {
    PacmanMachine *machine = context;
    return machine->dsw2;
}

static void coin_counter_write(
    void *context,
    bool active
) {
    PacmanMachine *machine = context;
    machine->coin_counter = active;
}

static void watchdog_kick(
    void *context
) {
    PacmanMachine *machine = context;
    machine->watchdog_kicks++;
}

static void sound_register_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    PacmanMachine *machine = context;

    if (
        machine == NULL ||
        offset >= PACMAN_SOUND_REGISTER_COUNT
    ) {
        return;
    }

    machine->pending_sound_registers[offset] =
        value;

    machine->pending_sound_register_mask |=
        UINT32_C(1) << offset;
}

static void sound_enable_write(
    void *context,
    bool enabled
) {
    PacmanMachine *machine = context;

    if (machine == NULL) {
        return;
    }

    /*
     * MAME's sound-enable line is independent of
     * pacman_sound_w() and does not perform the register
     * stream-update operation.
     */
    pacman_audio_board_set_enabled(
        &machine->audio_board,
        enabled
    );
}

static void apply_pending_sound_writes(
    PacmanMachine *machine
) {
    const uint32_t mask =
        machine->pending_sound_register_mask;

    if (mask == 0u) {
        return;
    }

    for (
        unsigned offset = 0;
        offset < PACMAN_SOUND_REGISTER_COUNT;
        offset++
    ) {
        if (
            (mask &
             (UINT32_C(1) << offset)) ==
            0u
        ) {
            continue;
        }

        pacman_audio_board_write_register(
            &machine->audio_board,
            (uint16_t)offset,
            machine->pending_sound_registers[offset]
        );
    }

    machine->pending_sound_register_mask = 0u;
}

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
) {
    if (machine == NULL) {
        return false;
    }

    memset(
        machine,
        0,
        sizeof(*machine)
    );

    /*
     * Original inputs are active-low.
     */
    machine->input0 = 0xff;
    machine->input1 = 0xff;
    machine->dsw1 = 0xff;
    machine->dsw2 = 0xff;

    if (
        !pacman_video_board_init(
            &machine->video_board,

            tile_rom,
            tile_rom_size,

            sprite_rom,
            sprite_rom_size,

            palette_prom,
            palette_prom_size,

            color_lookup_prom,
            color_lookup_prom_size
        )
    ) {
        return false;
    }

    if (
        !pacman_audio_board_init(
            &machine->audio_board,

            sound_waveform_prom,
            sound_waveform_prom_size,

            sound_timing_prom,
            sound_timing_prom_size
        )
    ) {
        return false;
    }

    const PacmanCpuBoardConnections connections = {
        .video = {
            .context =
                &machine->video_board,

            .video_ram_read =
                pacman_video_board_video_ram_read,

            .video_ram_write =
                pacman_video_board_video_ram_write,

            .color_ram_read =
                pacman_video_board_color_ram_read,

            .color_ram_write =
                pacman_video_board_color_ram_write,

            .sprite_attribute_read =
                pacman_video_board_sprite_attribute_read,

            .sprite_attribute_write =
                pacman_video_board_sprite_attribute_write,

            .sprite_coordinate_write =
                pacman_video_board_sprite_coordinate_write,

            .set_flip_screen =
                pacman_video_board_set_flip_screen
        },

        .sound = {
            .context =
                machine,

            .write_register =
                sound_register_write,

            .set_enabled =
                sound_enable_write
        },

        .io = {
            .context = machine,

            .read_input0 =
                read_input0,

            .read_input1 =
                read_input1,

            .read_dsw1 =
                read_dsw1,

            .read_dsw2 =
                read_dsw2,

            .coin_counter_write =
                coin_counter_write,

            .watchdog_kick =
                watchdog_kick
        }
    };

    if (
        !pacman_cpu_board_init(
            &machine->cpu_board,
            program_rom,
            program_rom_size,
            &connections
        )
    ) {
        return false;
    }

    pacman_video_timing_reset(
        &machine->video_timing
    );

    return true;
}

void pacman_machine_reset(
    PacmanMachine *machine
) {
    if (machine == NULL) {
        return;
    }

    pacman_audio_board_reset(
        &machine->audio_board
    );

    pacman_video_board_reset(
        &machine->video_board
    );

    pacman_video_timing_reset(
        &machine->video_timing
    );

    pacman_cpu_board_reset(
        &machine->cpu_board
    );

    machine->watchdog_kicks = 0;
    machine->coin_counter = false;

    machine->audio_t_state_accumulator = 0u;

    memset(
        machine->pending_sound_registers,
        0,
        sizeof(machine->pending_sound_registers)
    );

    machine->pending_sound_register_mask = 0u;
}

void pacman_machine_set_audio_sink(
    PacmanMachine *machine,
    PacmanMachineAudioSampleCallback callback,
    void *context
) {
    if (machine == NULL) {
        return;
    }

    machine->audio_sample_callback =
        callback;

    machine->audio_sample_context =
        context;
}

void pacman_machine_set_vblank(
    PacmanMachine *machine,
    bool active
) {
    if (machine == NULL) {
        return;
    }

    pacman_cpu_board_set_vblank(
        &machine->cpu_board,
        active
    );
}

uint32_t pacman_machine_step(
    PacmanMachine *machine
) {
    if (machine == NULL) {
        return 0u;
    }

    const bool previous_vblank =
        machine->video_timing.vblank;

    const uint32_t t_states =
        pacman_cpu_board_step(
            &machine->cpu_board
        );

    if (t_states == 0u) {
        return 0u;
    }

    pacman_video_timing_advance(
        &machine->video_timing,
        t_states *
            PACMAN_PIXELS_PER_CPU_T_STATE
    );

    /*
     * Advance the 192 kHz WSG clock from CPU T-states.
     *
     * Register writes happen during pacman_cpu_board_step(),
     * so timing is resolved at Z80 instruction granularity.
     */
    machine->audio_t_state_accumulator +=
        t_states;

    while (
        machine->audio_t_state_accumulator >=
        PACMAN_AUDIO_T_STATES_PER_SAMPLE
    ) {
        machine->audio_t_state_accumulator -=
            PACMAN_AUDIO_T_STATES_PER_SAMPLE;

        const int16_t sample =
            pacman_audio_board_next_sample(
                &machine->audio_board
            );

        if (
            machine->audio_sample_callback !=
            NULL
        ) {
            machine->audio_sample_callback(
                machine->audio_sample_context,
                sample
            );
        }
    }

    /*
     * Register changes become visible only after audio up to
     * this instruction boundary has been generated.
     */
    apply_pending_sound_writes(
        machine
    );

    /*
     * One Z80 instruction is far shorter than an entire
     * VBlank interval, so comparing before/after state is
     * sufficient to detect either edge.
     */
    if (
        machine->video_timing.vblank !=
        previous_vblank
    ) {
        pacman_machine_set_vblank(
            machine,
            machine->video_timing.vblank
        );
    }

    return t_states;
}

uint64_t pacman_machine_run(
    PacmanMachine *machine,
    uint64_t t_state_budget
) {
    if (machine == NULL) {
        return 0u;
    }

    uint64_t consumed = 0u;

    while (consumed < t_state_budget) {
        const uint32_t t_states =
            pacman_machine_step(
                machine
            );

        if (t_states == 0u) {
            break;
        }

        consumed += t_states;
    }

    return consumed;
}

PacmanRgb pacman_machine_video_pixel(
    const PacmanMachine *machine,
    uint16_t x,
    uint16_t y
) {
    const PacmanRgb black = {
        .r = 0,
        .g = 0,
        .b = 0
    };

    if (machine == NULL) {
        return black;
    }

    return pacman_video_board_output_pixel(
        &machine->video_board,
        x,
        y
    );
}
