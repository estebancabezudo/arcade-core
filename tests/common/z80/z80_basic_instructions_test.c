#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];

    uint16_t last_io_port;
    uint8_t last_io_value;
    uint32_t io_write_count;
} TestBus;

static uint8_t test_memory_read(
    void *context,
    uint16_t address
) {
    TestBus *bus = context;
    return bus->memory[address];
}

static void test_memory_write(
    void *context,
    uint16_t address,
    uint8_t value
) {
    TestBus *bus = context;
    bus->memory[address] = value;
}

static void test_io_write(
    void *context,
    uint16_t port,
    uint8_t value
) {
    TestBus *bus = context;

    bus->last_io_port = port;
    bus->last_io_value = value;
    bus->io_write_count++;
}

static void load_test_program(
    TestBus *bus
) {
    static const uint8_t program[] = {
        0xf3,                   /* DI */

        0x31, 0x00, 0x80,       /* LD SP,8000h */

        0x01, 0x34, 0x12,       /* LD BC,1234h */
        0x11, 0x78, 0x56,       /* LD DE,5678h */
        0x21, 0xbc, 0x9a,       /* LD HL,9ABCh */

        0x06, 0xaa,             /* LD B,AAh */
        0x0e, 0xbb,             /* LD C,BBh */

        0x16, 0xcc,             /* LD D,CCh */
        0x1e, 0xdd,             /* LD E,DDh */

        0x26, 0xee,             /* LD H,EEh */
        0x2e, 0xff,             /* LD L,FFh */

        0x3e, 0x42,             /* LD A,42h */

        0xd3, 0x10,             /* OUT (10h),A */

        0xfb,                   /* EI */
        0x00,                   /* NOP */
        0x76                    /* HALT */
    };

    for (
        uint32_t i = 0;
        i < sizeof(program);
        i++
    ) {
        bus->memory[i] = program[i];
    }
}

int main(void) {
    TestBus test_bus = {0};

    Z80Bus bus = {
        .context = &test_bus,
        .memory_read = test_memory_read,
        .memory_write = test_memory_write,
        .io_read = 0,
        .io_write = test_io_write,
        .interrupt_acknowledge = 0,
        .reti = 0
    };

    Z80 cpu;

    z80_init(
        &cpu,
        &bus
    );

    load_test_program(
        &test_bus
    );

    uint32_t executed_instructions = 0;

    while (!cpu.halted) {
        const uint32_t cycles =
            z80_step(&cpu);

        assert(cycles != 0);

        executed_instructions++;

        assert(executed_instructions < 100);
    }

    assert(executed_instructions == 16);

    assert(cpu.sp == 0x8000);

    assert(cpu.b == 0xaa);
    assert(cpu.c == 0xbb);

    assert(cpu.d == 0xcc);
    assert(cpu.e == 0xdd);

    assert(cpu.h == 0xee);
    assert(cpu.l == 0xff);

    assert(cpu.a == 0x42);

    /*
     * OUT (10h),A places A on A15..A8 and the immediate
     * operand on A7..A0.
     */
    assert(test_bus.io_write_count == 1);
    assert(test_bus.last_io_port == 0x4210);
    assert(test_bus.last_io_value == 0x42);

    /*
     * EI became effective after the following NOP.
     */
    assert(cpu.iff1);
    assert(cpu.iff2);
    assert(cpu.ei_delay == 0);

    assert(cpu.pc == 0x0020);

    /*
     * Expected T-states:
     *
     * DI                 4
     * LD SP,nn          10
     * LD BC,nn          10
     * LD DE,nn          10
     * LD HL,nn          10
     * six LD r,n        42
     * LD A,n             7
     * OUT (n),A         11
     * EI                 4
     * NOP                4
     * HALT               4
     *
     * Total            116
     */
    assert(cpu.total_t_states == 116);

    assert(cpu.r == 16);

    puts("PASS z80 basic instructions");

    return 0;
}
