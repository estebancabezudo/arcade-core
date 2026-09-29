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

static void test_register_to_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * LD B,C
     * LD A,B
     * HALT
     */
    bus.memory[0] = 0x41;
    bus.memory[1] = 0x78;
    bus.memory[2] = 0x76;

    cpu.c = 0x5a;

    assert(z80_step(&cpu) == 4);
    assert(cpu.b == 0x5a);

    assert(z80_step(&cpu) == 4);
    assert(cpu.a == 0x5a);

    assert(z80_step(&cpu) == 4);
    assert(cpu.halted);
}

static void test_register_to_memory(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * LD (HL),A
     * HALT
     */
    bus.memory[0] = 0x77;
    bus.memory[1] = 0x76;

    cpu.h = 0x40;
    cpu.l = 0x10;
    cpu.a = 0xa5;

    assert(z80_step(&cpu) == 7);
    assert(bus.memory[0x4010] == 0xa5);

    assert(z80_step(&cpu) == 4);
    assert(cpu.halted);
}

static void test_memory_to_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * LD A,(HL)
     * HALT
     */
    bus.memory[0] = 0x7e;
    bus.memory[1] = 0x76;

    cpu.h = 0x48;
    cpu.l = 0x20;

    bus.memory[0x4820] = 0x3c;

    assert(z80_step(&cpu) == 7);
    assert(cpu.a == 0x3c);

    assert(z80_step(&cpu) == 4);
    assert(cpu.halted);
}

static void test_immediate_to_memory(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * LD (HL),42h
     * HALT
     */
    bus.memory[0] = 0x36;
    bus.memory[1] = 0x42;
    bus.memory[2] = 0x76;

    cpu.h = 0x44;
    cpu.l = 0x00;

    assert(z80_step(&cpu) == 10);
    assert(bus.memory[0x4400] == 0x42);

    assert(z80_step(&cpu) == 4);
    assert(cpu.halted);
}

static void test_complete_register_matrix(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0x11;
    cpu.b = 0x22;
    cpu.c = 0x33;
    cpu.d = 0x44;
    cpu.e = 0x55;
    cpu.h = 0x66;
    cpu.l = 0x77;

    /*
     * LD C,A
     */
    bus.memory[0] = 0x4f;

    assert(z80_step(&cpu) == 4);
    assert(cpu.c == 0x11);

    /*
     * LD D,L
     */
    cpu.pc = 0;
    bus.memory[0] = 0x55;

    assert(z80_step(&cpu) == 4);
    assert(cpu.d == 0x77);

    /*
     * LD H,E
     */
    cpu.pc = 0;
    bus.memory[0] = 0x63;

    assert(z80_step(&cpu) == 4);
    assert(cpu.h == 0x55);
}

int main(void) {
    test_register_to_register();
    test_register_to_memory();
    test_memory_to_register();
    test_immediate_to_memory();
    test_complete_register_matrix();

    puts("PASS z80 loads");

    return 0;
}
