#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pacman_cpu_board.h"
#include "pacman_romset.h"

typedef struct {
    uint8_t video_ram[PACMAN_VIDEO_RAM_SIZE];
    uint8_t sound_registers[PACMAN_SOUND_REGISTER_COUNT];

    bool sound_enabled;
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

static void sound_register_write(
    void *context,
    uint16_t offset,
    uint8_t value
) {
    TestHardware *hardware = context;

    hardware->sound_registers[offset] = value;
}

static void sound_enable_write(
    void *context,
    bool enabled
) {
    TestHardware *hardware = context;

    hardware->sound_enabled = enabled;
}

static void build_synthetic_roms(
    uint8_t *chip0,
    uint8_t *chip1,
    uint8_t *chip2,
    uint8_t *chip3
) {
    memset(
        chip0,
        0,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );

    memset(
        chip1,
        0,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );

    memset(
        chip2,
        0,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );

    memset(
        chip3,
        0,
        PACMAN_PROGRAM_ROM_CHIP_SIZE
    );

    /*
     * 0000:
     *
     * LD SP,4FE0h
     * LD A,5Ah
     * LD (4C00h),A
     * JP 1000h
     */
    static const uint8_t program0[] = {
        0x31, 0xe0, 0x4f,

        0x3e, 0x5a,

        0x32, 0x00, 0x4c,

        0xc3, 0x00, 0x10
    };

    memcpy(
        chip0,
        program0,
        sizeof(program0)
    );

    /*
     * 1000:
     *
     * LD A,A5h
     * LD (4000h),A
     *
     * LD A,12h
     * LD (5040h),A
     *
     * LD A,01h
     * LD (5001h),A
     *
     * HALT
     */
    static const uint8_t program1[] = {
        0x3e, 0xa5,
        0x32, 0x00, 0x40,

        0x3e, 0x12,
        0x32, 0x40, 0x50,

        0x3e, 0x01,
        0x32, 0x01, 0x50,

        0x76
    };

    memcpy(
        chip1,
        program1,
        sizeof(program1)
    );
}

int main(void)
{
    uint8_t chip0[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip1[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip2[PACMAN_PROGRAM_ROM_CHIP_SIZE];
    uint8_t chip3[PACMAN_PROGRAM_ROM_CHIP_SIZE];

    build_synthetic_roms(
        chip0,
        chip1,
        chip2,
        chip3
    );

    PacmanRomSet romset;

    assert(
        pacman_romset_build_program(
            &romset,

            chip0,
            sizeof(chip0),

            chip1,
            sizeof(chip1),

            chip2,
            sizeof(chip2),

            chip3,
            sizeof(chip3)
        )
    );

    /*
     * Verify the physical chip boundary.
     */
    assert(romset.program[0x0000] == 0x31);
    assert(romset.program[0x1000] == 0x3e);

    TestHardware hardware = {0};

    const PacmanCpuBoardConnections connections = {
        .video = {
            .context = &hardware,

            .video_ram_read =
                video_ram_read,

            .video_ram_write =
                video_ram_write
        },

        .sound = {
            .context = &hardware,

            .write_register =
                sound_register_write,

            .set_enabled =
                sound_enable_write
        }
    };

    PacmanCpuBoard board;

    assert(
        pacman_cpu_board_init(
            &board,
            romset.program,
            sizeof(romset.program),
            &connections
        )
    );

    uint32_t instruction_count = 0;

    while (!board.cpu.halted) {
        const uint32_t cycles =
            pacman_cpu_board_step(
                &board
            );

        assert(cycles != 0);

        instruction_count++;

        assert(instruction_count < 100);
    }

    /*
     * Program from chip 0 wrote CPU-local RAM.
     */
    assert(board.work_ram[0] == 0x5a);

    /*
     * Execution crossed 0FFF/1000 into chip 1 and
     * reached the video board.
     */
    assert(hardware.video_ram[0] == 0xa5);

    /*
     * Same program reached the sound board.
     */
    assert(hardware.sound_registers[0] == 0x12);

    /*
     * And toggled output-latch bit 1.
     */
    assert(hardware.sound_enabled);

    /*
     * HALT is at 100Fh; PC points to 1010h after fetching it.
     */
    assert(board.cpu.pc == 0x1010);

    puts(
        "PASS Pac-Man ROM set -> CPU board -> Z80 integration"
    );

    return 0;
}
