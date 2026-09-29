#include "pacman_cpu_board.h"

#include <string.h>

enum {
    PACMAN_OPEN_BUS_VALUE = 0xbf,

    PACMAN_ROM_MIRROR = 0x8000,
    PACMAN_MEMORY_MIRROR = 0xa000,

    PACMAN_LATCH_MIRROR = 0xaf38,
    PACMAN_DEVICE_MIRROR = 0xaf00,
    PACMAN_INPUT_MIRROR = 0xaf3f
};

static uint16_t decode_address(
    uint16_t address,
    uint16_t mirror
) {
    return (uint16_t)(
        address &
        (uint16_t)(~mirror)
    );
}

static bool address_matches(
    uint16_t address,
    uint16_t start,
    uint16_t end,
    uint16_t mirror
) {
    const uint16_t decoded =
        decode_address(
            address,
            mirror
        );

    return
        decoded >= start &&
        decoded <= end;
}

static uint8_t read_video_ram(
    PacmanCpuBoard *board,
    uint16_t offset
) {
    if (
        board->connections.video.video_ram_read ==
        NULL
    ) {
        return PACMAN_OPEN_BUS_VALUE;
    }

    return
        board->connections.video.video_ram_read(
            board->connections.video.context,
            offset
        );
}

static void write_video_ram(
    PacmanCpuBoard *board,
    uint16_t offset,
    uint8_t value
) {
    if (
        board->connections.video.video_ram_write ==
        NULL
    ) {
        return;
    }

    board->connections.video.video_ram_write(
        board->connections.video.context,
        offset,
        value
    );
}

static uint8_t read_color_ram(
    PacmanCpuBoard *board,
    uint16_t offset
) {
    if (
        board->connections.video.color_ram_read ==
        NULL
    ) {
        return PACMAN_OPEN_BUS_VALUE;
    }

    return
        board->connections.video.color_ram_read(
            board->connections.video.context,
            offset
        );
}

static void write_color_ram(
    PacmanCpuBoard *board,
    uint16_t offset,
    uint8_t value
) {
    if (
        board->connections.video.color_ram_write ==
        NULL
    ) {
        return;
    }

    board->connections.video.color_ram_write(
        board->connections.video.context,
        offset,
        value
    );
}

static uint8_t read_sprite_attribute(
    PacmanCpuBoard *board,
    uint16_t offset
) {
    if (
        board->connections.video.sprite_attribute_read ==
        NULL
    ) {
        return PACMAN_OPEN_BUS_VALUE;
    }

    return
        board->connections.video.sprite_attribute_read(
            board->connections.video.context,
            offset
        );
}

static void write_sprite_attribute(
    PacmanCpuBoard *board,
    uint16_t offset,
    uint8_t value
) {
    if (
        board->connections.video.sprite_attribute_write ==
        NULL
    ) {
        return;
    }

    board->connections.video.sprite_attribute_write(
        board->connections.video.context,
        offset,
        value
    );
}

static uint8_t read_input(
    uint8_t (*reader)(void *),
    void *context
) {
    if (reader == NULL) {
        return 0xff;
    }

    return reader(context);
}

static void update_output_latch(
    PacmanCpuBoard *board,
    uint8_t bit,
    bool active
) {
    const uint8_t mask =
        (uint8_t)(
            1u << bit
        );

    const bool previous =
        (board->output_latch & mask) != 0u;

    if (active) {
        board->output_latch |= mask;
    } else {
        board->output_latch &=
            (uint8_t)~mask;
    }

    if (previous == active) {
        return;
    }

    switch (bit) {
        /*
         * Interrupt enable.
         *
         * Disabling it clears the IRQ line immediately.
         * Enabling it does not synthesize a new VBlank edge.
         */
        case 0:
            if (!active) {
                z80_set_irq(
                    &board->cpu,
                    false
                );
            }
            break;

        /*
         * Sound enable.
         */
        case 1:
            if (
                board->connections.sound.set_enabled !=
                NULL
            ) {
                board->connections.sound.set_enabled(
                    board->connections.sound.context,
                    active
                );
            }
            break;

        /*
         * Bit 2 is not connected on original Pac-Man.
         */
        case 2:
            break;

        /*
         * Flip screen.
         */
        case 3:
            if (
                board->connections.video.set_flip_screen !=
                NULL
            ) {
                board->connections.video.set_flip_screen(
                    board->connections.video.context,
                    active
                );
            }
            break;

        /*
         * Bits 4 and 5 are lamp outputs in the logical
         * latch map but are not exposed by this board API.
         */
        case 4:
        case 5:
            break;

        /*
         * Bit 6 is unused here.
         */
        case 6:
            break;

        /*
         * Coin counter.
         */
        case 7:
            if (
                board->connections.io.coin_counter_write !=
                NULL
            ) {
                board->connections.io.coin_counter_write(
                    board->connections.io.context,
                    active
                );
            }
            break;

        default:
            break;
    }
}

uint8_t pacman_cpu_board_memory_read(
    PacmanCpuBoard *board,
    uint16_t address
) {
    /*
     * 0000-3FFF ROM, mirrored at 8000-BFFF.
     */
    if (
        address_matches(
            address,
            0x0000,
            0x3fff,
            PACMAN_ROM_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_ROM_MIRROR
            );

        return board->program_rom[decoded];
    }

    /*
     * 4000-43FF video RAM.
     */
    if (
        address_matches(
            address,
            0x4000,
            0x43ff,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_MEMORY_MIRROR
            );

        return read_video_ram(
            board,
            (uint16_t)(
                decoded - 0x4000
            )
        );
    }

    /*
     * 4400-47FF color RAM.
     */
    if (
        address_matches(
            address,
            0x4400,
            0x47ff,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_MEMORY_MIRROR
            );

        return read_color_ram(
            board,
            (uint16_t)(
                decoded - 0x4400
            )
        );
    }

    /*
     * 4800-4BFF has no enabled memory device on Pac-Man.
     *
     * MAME documents 0xBF as the observed/common floating-bus
     * value for this region.
     */
    if (
        address_matches(
            address,
            0x4800,
            0x4bff,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        return PACMAN_OPEN_BUS_VALUE;
    }

    /*
     * 4C00-4FEF work RAM.
     */
    if (
        address_matches(
            address,
            0x4c00,
            0x4fef,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_MEMORY_MIRROR
            );

        return
            board->work_ram[
                decoded - 0x4c00
            ];
    }

    /*
     * 4FF0-4FFF sprite attributes.
     */
    if (
        address_matches(
            address,
            0x4ff0,
            0x4fff,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_MEMORY_MIRROR
            );

        return read_sprite_attribute(
            board,
            (uint16_t)(
                decoded - 0x4ff0
            )
        );
    }

    /*
     * Memory-mapped inputs.
     *
     * The large mirror mask reflects address lines that are not
     * decoded by the original board.
     */
    if (
        address_matches(
            address,
            0x5000,
            0x5000,
            PACMAN_INPUT_MIRROR
        )
    ) {
        return read_input(
            board->connections.io.read_input0,
            board->connections.io.context
        );
    }

    if (
        address_matches(
            address,
            0x5040,
            0x5040,
            PACMAN_INPUT_MIRROR
        )
    ) {
        return read_input(
            board->connections.io.read_input1,
            board->connections.io.context
        );
    }

    if (
        address_matches(
            address,
            0x5080,
            0x5080,
            PACMAN_INPUT_MIRROR
        )
    ) {
        return read_input(
            board->connections.io.read_dsw1,
            board->connections.io.context
        );
    }

    if (
        address_matches(
            address,
            0x50c0,
            0x50c0,
            PACMAN_INPUT_MIRROR
        )
    ) {
        return read_input(
            board->connections.io.read_dsw2,
            board->connections.io.context
        );
    }

    return PACMAN_OPEN_BUS_VALUE;
}

void pacman_cpu_board_memory_write(
    PacmanCpuBoard *board,
    uint16_t address,
    uint8_t value
) {
    /*
     * ROM writes have no effect.
     */
    if (
        address_matches(
            address,
            0x0000,
            0x3fff,
            PACMAN_ROM_MIRROR
        )
    ) {
        return;
    }

    if (
        address_matches(
            address,
            0x4000,
            0x43ff,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_MEMORY_MIRROR
            );

        write_video_ram(
            board,
            (uint16_t)(
                decoded - 0x4000
            ),
            value
        );

        return;
    }

    if (
        address_matches(
            address,
            0x4400,
            0x47ff,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_MEMORY_MIRROR
            );

        write_color_ram(
            board,
            (uint16_t)(
                decoded - 0x4400
            ),
            value
        );

        return;
    }

    /*
     * 4800-4BFF writes are ignored.
     */
    if (
        address_matches(
            address,
            0x4800,
            0x4bff,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        return;
    }

    if (
        address_matches(
            address,
            0x4c00,
            0x4fef,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_MEMORY_MIRROR
            );

        board->work_ram[
            decoded - 0x4c00
        ] = value;

        return;
    }

    if (
        address_matches(
            address,
            0x4ff0,
            0x4fff,
            PACMAN_MEMORY_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_MEMORY_MIRROR
            );

        write_sprite_attribute(
            board,
            (uint16_t)(
                decoded - 0x4ff0
            ),
            value
        );

        return;
    }

    /*
     * 5000-5007: address selects one bit of the output latch.
     * Data bit 0 determines the new state.
     */
    if (
        address_matches(
            address,
            0x5000,
            0x5007,
            PACMAN_LATCH_MIRROR
        )
    ) {
        const uint16_t decoded =
            decode_address(
                address,
                PACMAN_LATCH_MIRROR
            );

        const uint8_t bit =
            (uint8_t)(
                decoded & 7u
            );

        update_output_latch(
            board,
            bit,
            (value & 1u) != 0u
        );

        return;
    }

    /*
     * 5040-505F sound registers.
     */
    if (
        address_matches(
            address,
            0x5040,
            0x505f,
            PACMAN_DEVICE_MIRROR
        )
    ) {
        if (
            board->connections.sound.write_register !=
            NULL
        ) {
            const uint16_t decoded =
                decode_address(
                    address,
                    PACMAN_DEVICE_MIRROR
                );

            board->connections.sound.write_register(
                board->connections.sound.context,
                (uint16_t)(
                    decoded - 0x5040
                ),
                value
            );
        }

        return;
    }

    /*
     * 5060-506F sprite X/Y coordinates.
     */
    if (
        address_matches(
            address,
            0x5060,
            0x506f,
            PACMAN_DEVICE_MIRROR
        )
    ) {
        if (
            board->connections.video.sprite_coordinate_write !=
            NULL
        ) {
            const uint16_t decoded =
                decode_address(
                    address,
                    PACMAN_DEVICE_MIRROR
                );

            board->connections.video.sprite_coordinate_write(
                board->connections.video.context,
                (uint16_t)(
                    decoded - 0x5060
                ),
                value
            );
        }

        return;
    }

    /*
     * 5070-507F: no connected write device.
     */
    if (
        address_matches(
            address,
            0x5070,
            0x507f,
            PACMAN_DEVICE_MIRROR
        )
    ) {
        return;
    }

    /*
     * 5080 write is unused on original Pac-Man.
     */
    if (
        address_matches(
            address,
            0x5080,
            0x5080,
            PACMAN_INPUT_MIRROR
        )
    ) {
        return;
    }

    /*
     * 50C0 resets the watchdog.
     */
    if (
        address_matches(
            address,
            0x50c0,
            0x50c0,
            PACMAN_INPUT_MIRROR
        )
    ) {
        if (
            board->connections.io.watchdog_kick !=
            NULL
        ) {
            board->connections.io.watchdog_kick(
                board->connections.io.context
            );
        }

        return;
    }
}

uint8_t pacman_cpu_board_io_read(
    PacmanCpuBoard *board,
    uint16_t port
) {
    (void)board;
    (void)port;

    /*
     * Original Pac-Man does not expose an I/O read device
     * in the CPU port map.
     */
    return 0xff;
}

void pacman_cpu_board_io_write(
    PacmanCpuBoard *board,
    uint16_t port,
    uint8_t value
) {
    /*
     * The original port map only decodes the low 8 address bits.
     *
     * OUT to port 00 loads the IM 0 interrupt vector latch.
     */
    if ((port & 0x00ffu) == 0u) {
        board->interrupt_vector = value;
    }
}

static uint8_t z80_memory_read_callback(
    void *context,
    uint16_t address
) {
    return pacman_cpu_board_memory_read(
        context,
        address
    );
}

static void z80_memory_write_callback(
    void *context,
    uint16_t address,
    uint8_t value
) {
    pacman_cpu_board_memory_write(
        context,
        address,
        value
    );
}

static uint8_t z80_io_read_callback(
    void *context,
    uint16_t port
) {
    return pacman_cpu_board_io_read(
        context,
        port
    );
}

static void z80_io_write_callback(
    void *context,
    uint16_t port,
    uint8_t value
) {
    pacman_cpu_board_io_write(
        context,
        port,
        value
    );
}

static uint8_t z80_interrupt_acknowledge_callback(
    void *context
) {
    PacmanCpuBoard *board =
        context;

    return board->interrupt_vector;
}

static void z80_reti_callback(
    void *context
) {
    (void)context;
}

bool pacman_cpu_board_init(
    PacmanCpuBoard *board,
    const uint8_t *program_rom,
    size_t program_rom_size,
    const PacmanCpuBoardConnections *connections
) {
    if (
        board == NULL ||
        program_rom == NULL ||
        program_rom_size !=
            PACMAN_PROGRAM_ROM_SIZE
    ) {
        return false;
    }

    memset(
        board,
        0,
        sizeof(*board)
    );

    board->program_rom =
        program_rom;

    board->program_rom_size =
        program_rom_size;

    if (connections != NULL) {
        board->connections =
            *connections;
    }

    const Z80Bus bus = {
        .context = board,

        .memory_read =
            z80_memory_read_callback,

        .memory_write =
            z80_memory_write_callback,

        .io_read =
            z80_io_read_callback,

        .io_write =
            z80_io_write_callback,

        .interrupt_acknowledge =
            z80_interrupt_acknowledge_callback,

        .reti =
            z80_reti_callback
    };

    z80_init(
        &board->cpu,
        &bus
    );

    return true;
}

void pacman_cpu_board_reset(
    PacmanCpuBoard *board
) {
    if (board == NULL) {
        return;
    }

    /*
     * SRAM contents are deliberately preserved.
     *
     * A hardware CPU reset does not clear external RAM.
     */
    board->output_latch = 0u;
    board->interrupt_vector = 0u;
    board->vblank = false;

    z80_reset(
        &board->cpu
    );

    z80_set_irq(
        &board->cpu,
        false
    );

    if (
        board->connections.sound.set_enabled !=
        NULL
    ) {
        board->connections.sound.set_enabled(
            board->connections.sound.context,
            false
        );
    }

    if (
        board->connections.video.set_flip_screen !=
        NULL
    ) {
        board->connections.video.set_flip_screen(
            board->connections.video.context,
            false
        );
    }

    if (
        board->connections.io.coin_counter_write !=
        NULL
    ) {
        board->connections.io.coin_counter_write(
            board->connections.io.context,
            false
        );
    }
}

uint32_t pacman_cpu_board_step(
    PacmanCpuBoard *board
) {
    if (board == NULL) {
        return 0u;
    }

    return z80_step(
        &board->cpu
    );
}

uint64_t pacman_cpu_board_run(
    PacmanCpuBoard *board,
    uint64_t t_state_budget
) {
    if (board == NULL) {
        return 0u;
    }

    uint64_t consumed = 0u;

    while (consumed < t_state_budget) {
        const uint32_t step =
            pacman_cpu_board_step(
                board
            );

        if (step == 0u) {
            break;
        }

        consumed += step;
    }

    return consumed;
}

void pacman_cpu_board_set_vblank(
    PacmanCpuBoard *board,
    bool active
) {
    if (board == NULL) {
        return;
    }

    const bool rising_edge =
        active &&
        !board->vblank;

    board->vblank = active;

    /*
     * Pac-Man asserts IRQ on the VBlank rising edge only when
     * output-latch bit 0 has enabled interrupts.
     */
    if (
        rising_edge &&
        (board->output_latch & 0x01u) != 0u
    ) {
        z80_set_irq(
            &board->cpu,
            true
        );
    }
}
