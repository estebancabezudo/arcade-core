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

static void test_push_bc(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * PUSH BC
     */
    bus.memory[0x0000] = 0xc5;

    cpu.sp = 0x9000;

    cpu.b = 0x12;
    cpu.c = 0x34;

    assert(z80_step(&cpu) == 11);

    assert(cpu.sp == 0x8ffe);

    assert(bus.memory[0x8fff] == 0x12);
    assert(bus.memory[0x8ffe] == 0x34);
}

static void test_pop_bc(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * POP BC
     */
    bus.memory[0x0000] = 0xc1;

    cpu.sp = 0x8000;

    bus.memory[0x8000] = 0x78;
    bus.memory[0x8001] = 0x56;

    assert(z80_step(&cpu) == 10);

    assert(cpu.b == 0x56);
    assert(cpu.c == 0x78);

    assert(cpu.sp == 0x8002);
}

static void test_push_pop_all_pairs(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * PUSH BC
     * PUSH DE
     * PUSH HL
     * PUSH AF
     *
     * Clear registers manually.
     *
     * POP AF
     * POP HL
     * POP DE
     * POP BC
     */
    bus.memory[0x0000] = 0xc5;
    bus.memory[0x0001] = 0xd5;
    bus.memory[0x0002] = 0xe5;
    bus.memory[0x0003] = 0xf5;

    bus.memory[0x0004] = 0xf1;
    bus.memory[0x0005] = 0xe1;
    bus.memory[0x0006] = 0xd1;
    bus.memory[0x0007] = 0xc1;

    cpu.sp = 0x9000;

    cpu.b = 0x11;
    cpu.c = 0x22;

    cpu.d = 0x33;
    cpu.e = 0x44;

    cpu.h = 0x55;
    cpu.l = 0x66;

    cpu.a = 0x77;
    cpu.f = 0x88;

    assert(z80_step(&cpu) == 11);
    assert(z80_step(&cpu) == 11);
    assert(z80_step(&cpu) == 11);
    assert(z80_step(&cpu) == 11);

    assert(cpu.sp == 0x8ff8);

    cpu.b = 0;
    cpu.c = 0;

    cpu.d = 0;
    cpu.e = 0;

    cpu.h = 0;
    cpu.l = 0;

    cpu.a = 0;
    cpu.f = 0;

    assert(z80_step(&cpu) == 10);

    assert(cpu.a == 0x77);
    assert(cpu.f == 0x88);

    assert(z80_step(&cpu) == 10);

    assert(cpu.h == 0x55);
    assert(cpu.l == 0x66);

    assert(z80_step(&cpu) == 10);

    assert(cpu.d == 0x33);
    assert(cpu.e == 0x44);

    assert(z80_step(&cpu) == 10);

    assert(cpu.b == 0x11);
    assert(cpu.c == 0x22);

    assert(cpu.sp == 0x9000);

    assert(cpu.total_t_states == 84);
}

int main(void) {
    test_push_bc();
    test_pop_bc();
    test_push_pop_all_pairs();

    puts("PASS z80 stack");

    return 0;
}
