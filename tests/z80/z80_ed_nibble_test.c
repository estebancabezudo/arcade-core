#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];
} TestBus;

static uint8_t memory_read(
    void *context,
    uint16_t address
) {
    TestBus *bus = context;
    return bus->memory[address];
}

static void memory_write(
    void *context,
    uint16_t address,
    uint8_t value
) {
    TestBus *bus = context;
    bus->memory[address] = value;
}

static Z80 create_cpu(
    TestBus *test_bus
) {
    Z80 cpu;

    const Z80Bus bus = {
        .context = test_bus,
        .memory_read = memory_read,
        .memory_write = memory_write,
        .io_read = 0,
        .io_write = 0,
        .interrupt_acknowledge = 0,
        .reti = 0
    };

    z80_init(&cpu, &bus);

    return cpu;
}

static void test_rrd(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0xed;
    bus.memory[1] = 0x67;

    cpu.h = 0x40;
    cpu.l = 0x00;

    cpu.a = 0x12;

    bus.memory[0x4000] = 0x34;

    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 18);

    /*
     * A: 12 -> 14
     * M: 34 -> 23
     */
    assert(cpu.a == 0x14);
    assert(bus.memory[0x4000] == 0x23);

    assert(cpu.f & Z80_FLAG_C);
    assert(!(cpu.f & Z80_FLAG_H));
    assert(!(cpu.f & Z80_FLAG_N));
}

static void test_rld(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0xed;
    bus.memory[1] = 0x6f;

    cpu.h = 0x40;
    cpu.l = 0x00;

    cpu.a = 0x12;

    bus.memory[0x4000] = 0x34;

    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 18);

    /*
     * A: 12 -> 13
     * M: 34 -> 42
     */
    assert(cpu.a == 0x13);
    assert(bus.memory[0x4000] == 0x42);

    assert(cpu.f & Z80_FLAG_C);
}

int main(void) {
    test_rrd();
    test_rld();

    puts("PASS z80 ED RRD/RLD");

    return 0;
}
