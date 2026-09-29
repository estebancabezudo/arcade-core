#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pacman_cpu_board.h"

typedef struct {
    uint8_t video_ram[PACMAN_VIDEO_RAM_SIZE];
    uint8_t color_ram[PACMAN_COLOR_RAM_SIZE];

    uint8_t sprite_attribute_ram[
        PACMAN_SPRITE_ATTRIBUTE_RAM_SIZE
    ];

    uint8_t sprite_coordinate_ram[
        PACMAN_SPRITE_COORDINATE_RAM_SIZE
    ];

    uint8_t sound_registers[
        PACMAN_SOUND_REGISTER_COUNT
    ];

    bool sound_enabled;
    bool flip_screen;
    bool coin_counter;

    uint32_t watchdog_kicks;

    uint8_t input0;
    uint8_t input1;
    uint8_t dsw1;
    uint8_t dsw2;
} TestHardware;

static uint8_t video_ram_read(
    void *context,
    uint16_t offset
) {
    TestHardware *hardware = context;
    return hardware->video_ram[offset];
}

static void video_ram_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    TestHardware *hardware = context;
    hardware->video_ram[offset] = value;
}

static uint8_t color_ram_read(
    void *context,
    uint16_t offset
) {
    TestHardware *hardware = context;
    return hardware->color_ram[offset];
}

static void color_ram_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    TestHardware *hardware = context;
    hardware->color_ram[offset] = value;
}

static uint8_t sprite_attribute_read(
    void *context,
    uint16_t offset
) {
    TestHardware *hardware = context;

    return
        hardware->sprite_attribute_ram[offset];
}

static void sprite_attribute_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    TestHardware *hardware = context;

    hardware->sprite_attribute_ram[offset] =
        value;
}

static void sprite_coordinate_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    TestHardware *hardware = context;

    hardware->sprite_coordinate_ram[offset] =
        value;
}

static void flip_screen_write(
    void *context,
    bool enabled
) {
    TestHardware *hardware = context;
    hardware->flip_screen = enabled;
}

static void sound_register_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    TestHardware *hardware = context;

    hardware->sound_registers[offset] =
        value;
}

static void sound_enable_write(
    void *context,
    bool enabled
) {
    TestHardware *hardware = context;
    hardware->sound_enabled = enabled;
}

static uint8_t read_input0(
    void *context
) {
    TestHardware *hardware = context;
    return hardware->input0;
}

static uint8_t read_input1(
    void *context
) {
    TestHardware *hardware = context;
    return hardware->input1;
}

static uint8_t read_dsw1(
    void *context
) {
    TestHardware *hardware = context;
    return hardware->dsw1;
}

static uint8_t read_dsw2(
    void *context
) {
    TestHardware *hardware = context;
    return hardware->dsw2;
}

static void coin_counter_write(
    void *context,
    bool active
) {
    TestHardware *hardware = context;
    hardware->coin_counter = active;
}

static void watchdog_kick(
    void *context
) {
    TestHardware *hardware = context;
    hardware->watchdog_kicks++;
}

static PacmanCpuBoardConnections
create_connections(
    TestHardware *hardware
) {
    PacmanCpuBoardConnections connections = {
        .video = {
            .context = hardware,

            .video_ram_read =
                video_ram_read,

            .video_ram_write =
                video_ram_write,

            .color_ram_read =
                color_ram_read,

            .color_ram_write =
                color_ram_write,

            .sprite_attribute_read =
                sprite_attribute_read,

            .sprite_attribute_write =
                sprite_attribute_write,

            .sprite_coordinate_write =
                sprite_coordinate_write,

            .set_flip_screen =
                flip_screen_write
        },

        .sound = {
            .context = hardware,

            .write_register =
                sound_register_write,

            .set_enabled =
                sound_enable_write
        },

        .io = {
            .context = hardware,

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

    return connections;
}

static void test_initialization(void)
{
    static uint8_t rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    TestHardware hardware = {0};

    const PacmanCpuBoardConnections connections =
        create_connections(
            &hardware
        );

    PacmanCpuBoard board;

    assert(
        !pacman_cpu_board_init(
            &board,
            rom,
            PACMAN_PROGRAM_ROM_SIZE - 1u,
            &connections
        )
    );

    assert(
        pacman_cpu_board_init(
            &board,
            rom,
            sizeof(rom),
            &connections
        )
    );

    assert(board.cpu.pc == 0);
    assert(board.output_latch == 0);
    assert(board.interrupt_vector == 0);
}

static void test_rom_and_mirrors(void)
{
    static uint8_t rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    memset(
        rom,
        0,
        sizeof(rom)
    );

    rom[0x1234] = 0x5a;

    TestHardware hardware = {0};

    const PacmanCpuBoardConnections connections =
        create_connections(
            &hardware
        );

    PacmanCpuBoard board;

    assert(
        pacman_cpu_board_init(
            &board,
            rom,
            sizeof(rom),
            &connections
        )
    );

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x1234
        ) == 0x5a
    );

    /*
     * A15 is not decoded for ROM.
     */
    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x9234
        ) == 0x5a
    );

    pacman_cpu_board_memory_write(
        &board,
        0x1234,
        0xff
    );

    assert(rom[0x1234] == 0x5a);
}

static void test_video_memory(void)
{
    static uint8_t rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    TestHardware hardware = {0};

    const PacmanCpuBoardConnections connections =
        create_connections(
            &hardware
        );

    PacmanCpuBoard board;

    assert(
        pacman_cpu_board_init(
            &board,
            rom,
            sizeof(rom),
            &connections
        )
    );

    pacman_cpu_board_memory_write(
        &board,
        0x4002,
        0x11
    );

    assert(hardware.video_ram[2] == 0x11);

    /*
     * A15/A13 mirrors.
     */
    pacman_cpu_board_memory_write(
        &board,
        0xe002,
        0x22
    );

    assert(hardware.video_ram[2] == 0x22);

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x4002
        ) == 0x22
    );

    pacman_cpu_board_memory_write(
        &board,
        0x4403,
        0x33
    );

    assert(hardware.color_ram[3] == 0x33);

    pacman_cpu_board_memory_write(
        &board,
        0xe403,
        0x44
    );

    assert(hardware.color_ram[3] == 0x44);
}

static void test_work_and_sprite_ram(void)
{
    static uint8_t rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    TestHardware hardware = {0};

    const PacmanCpuBoardConnections connections =
        create_connections(
            &hardware
        );

    PacmanCpuBoard board;

    assert(
        pacman_cpu_board_init(
            &board,
            rom,
            sizeof(rom),
            &connections
        )
    );

    pacman_cpu_board_memory_write(
        &board,
        0x4c10,
        0xa5
    );

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0xec10
        ) == 0xa5
    );

    pacman_cpu_board_memory_write(
        &board,
        0x4ff2,
        0x66
    );

    assert(
        hardware.sprite_attribute_ram[2] ==
        0x66
    );

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0xeff2
        ) == 0x66
    );

    /*
     * Floating/unconnected RAM area.
     */
    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x4800
        ) == 0xbf
    );
}

static void test_inputs(void)
{
    static uint8_t rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    TestHardware hardware = {
        .input0 = 0xfe,
        .input1 = 0xfd,
        .dsw1 = 0xfb,
        .dsw2 = 0xf7
    };

    const PacmanCpuBoardConnections connections =
        create_connections(
            &hardware
        );

    PacmanCpuBoard board;

    assert(
        pacman_cpu_board_init(
            &board,
            rom,
            sizeof(rom),
            &connections
        )
    );

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x5000
        ) == 0xfe
    );

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x503f
        ) == 0xfe
    );

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x5040
        ) == 0xfd
    );

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x5080
        ) == 0xfb
    );

    assert(
        pacman_cpu_board_memory_read(
            &board,
            0x50c0
        ) == 0xf7
    );
}

static void test_latch_and_devices(void)
{
    static uint8_t rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    TestHardware hardware = {0};

    const PacmanCpuBoardConnections connections =
        create_connections(
            &hardware
        );

    PacmanCpuBoard board;

    assert(
        pacman_cpu_board_init(
            &board,
            rom,
            sizeof(rom),
            &connections
        )
    );

    /*
     * Sound enable.
     */
    pacman_cpu_board_memory_write(
        &board,
        0x5001,
        1
    );

    assert(hardware.sound_enabled);

    pacman_cpu_board_memory_write(
        &board,
        0xd001,
        0
    );

    assert(!hardware.sound_enabled);

    /*
     * Flip screen.
     */
    pacman_cpu_board_memory_write(
        &board,
        0x5003,
        1
    );

    assert(hardware.flip_screen);

    /*
     * Coin counter.
     */
    pacman_cpu_board_memory_write(
        &board,
        0x5007,
        1
    );

    assert(hardware.coin_counter);

    /*
     * Sound register window.
     */
    pacman_cpu_board_memory_write(
        &board,
        0x5040,
        0x12
    );

    pacman_cpu_board_memory_write(
        &board,
        0x505f,
        0x34
    );

    assert(
        hardware.sound_registers[0x00] ==
        0x12
    );

    assert(
        hardware.sound_registers[0x1f] ==
        0x34
    );

    /*
     * Sprite coordinate RAM.
     */
    pacman_cpu_board_memory_write(
        &board,
        0x5060,
        0x56
    );

    pacman_cpu_board_memory_write(
        &board,
        0x506f,
        0x78
    );

    assert(
        hardware.sprite_coordinate_ram[0] ==
        0x56
    );

    assert(
        hardware.sprite_coordinate_ram[15] ==
        0x78
    );

    /*
     * Watchdog.
     */
    pacman_cpu_board_memory_write(
        &board,
        0x50c0,
        0
    );

    assert(hardware.watchdog_kicks == 1);
}

static void test_interrupt_vector_and_vblank(void)
{
    static uint8_t rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    memset(
        rom,
        0,
        sizeof(rom)
    );

    /*
     * 0000: EI
     * 0001: NOP
     */
    rom[0x0000] = 0xfb;
    rom[0x0001] = 0x00;

    TestHardware hardware = {0};

    const PacmanCpuBoardConnections connections =
        create_connections(
            &hardware
        );

    PacmanCpuBoard board;

    assert(
        pacman_cpu_board_init(
            &board,
            rom,
            sizeof(rom),
            &connections
        )
    );

    board.cpu.sp = 0x4fe0;

    /*
     * OUT on any port whose low byte is zero
     * loads the interrupt vector.
     *
     * CF = RST 08h.
     */
    pacman_cpu_board_io_write(
        &board,
        0xab00,
        0xcf
    );

    assert(board.interrupt_vector == 0xcf);

    /*
     * Enable interrupt latch.
     */
    pacman_cpu_board_memory_write(
        &board,
        0x5000,
        1
    );

    assert(
        (board.output_latch & 1u) != 0u
    );

    /*
     * EI and following NOP.
     */
    assert(
        pacman_cpu_board_step(
            &board
        ) == 4
    );

    assert(
        pacman_cpu_board_step(
            &board
        ) == 4
    );

    assert(board.cpu.iff1);

    /*
     * Rising VBlank drives INT.
     */
    pacman_cpu_board_set_vblank(
        &board,
        true
    );

    assert(board.cpu.irq_line);

    /*
     * IM0 receives CF from the external vector latch:
     * RST 08h.
     */
    assert(
        pacman_cpu_board_step(
            &board
        ) == 13
    );

    assert(board.cpu.pc == 0x0008);
    assert(board.cpu.sp == 0x4fde);

    /*
     * Return PC was 0002h.
     */
    assert(
        board.work_ram[0x03de] ==
        0x02
    );

    assert(
        board.work_ram[0x03df] ==
        0x00
    );

    /*
     * Disabling interrupt latch clears the line.
     */
    pacman_cpu_board_memory_write(
        &board,
        0x5000,
        0
    );

    assert(!board.cpu.irq_line);
}

static void test_reset_and_run_budget(void)
{
    static uint8_t rom[
        PACMAN_PROGRAM_ROM_SIZE
    ];

    memset(
        rom,
        0,
        sizeof(rom)
    );

    TestHardware hardware = {0};

    const PacmanCpuBoardConnections connections =
        create_connections(
            &hardware
        );

    PacmanCpuBoard board;

    assert(
        pacman_cpu_board_init(
            &board,
            rom,
            sizeof(rom),
            &connections
        )
    );

    board.work_ram[0] = 0x5a;

    pacman_cpu_board_memory_write(
        &board,
        0x5001,
        1
    );

    pacman_cpu_board_reset(
        &board
    );

    /*
     * CPU reset does not clear external SRAM.
     */
    assert(board.work_ram[0] == 0x5a);

    assert(board.cpu.pc == 0);
    assert(board.output_latch == 0);

    assert(!hardware.sound_enabled);
    assert(!hardware.flip_screen);
    assert(!hardware.coin_counter);

    /*
     * ROM contains NOPs: 4 T-states each.
     *
     * Budget 10 therefore consumes 12.
     */
    const uint64_t consumed =
        pacman_cpu_board_run(
            &board,
            10
        );

    assert(consumed == 12);
    assert(board.cpu.pc == 3);
}

int main(void)
{
    test_initialization();

    test_rom_and_mirrors();

    test_video_memory();
    test_work_and_sprite_ram();

    test_inputs();
    test_latch_and_devices();

    test_interrupt_vector_and_vblank();

    test_reset_and_run_budget();

    puts(
        "PASS Pac-Man CPU board decoder, I/O and interrupt wiring"
    );

    return 0;
}
