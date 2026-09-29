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

static void test_inc_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * INC B
     */
    bus.memory[0] = 0x04;

    cpu.b = 0x0f;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 4);

    assert(cpu.b == 0x10);

    assert(!(cpu.f & Z80_FLAG_S));
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_H);
    assert(!(cpu.f & Z80_FLAG_PV));
    assert(!(cpu.f & Z80_FLAG_N));

    /*
     * Carry must be preserved.
     */
    assert(cpu.f & Z80_FLAG_C);
}

static void test_inc_overflow(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * INC A
     */
    bus.memory[0] = 0x3c;

    cpu.a = 0x7f;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x80);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);
    assert(!(cpu.f & Z80_FLAG_N));
}

static void test_inc_zero(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0x0c; /* INC C */

    cpu.c = 0xff;

    assert(z80_step(&cpu) == 4);

    assert(cpu.c == 0x00);
    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_H);
}

static void test_dec_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * DEC D
     */
    bus.memory[0] = 0x15;

    cpu.d = 0x10;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 4);

    assert(cpu.d == 0x0f);

    assert(!(cpu.f & Z80_FLAG_S));
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_H);
    assert(!(cpu.f & Z80_FLAG_PV));
    assert(cpu.f & Z80_FLAG_N);

    assert(cpu.f & Z80_FLAG_C);
}

static void test_dec_overflow(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * DEC E
     */
    bus.memory[0] = 0x1d;

    cpu.e = 0x80;

    assert(z80_step(&cpu) == 4);

    assert(cpu.e == 0x7f);

    assert(!(cpu.f & Z80_FLAG_S));
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);
    assert(cpu.f & Z80_FLAG_N);
}

static void test_inc_memory(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * INC (HL)
     */
    bus.memory[0] = 0x34;

    cpu.h = 0x40;
    cpu.l = 0x00;

    bus.memory[0x4000] = 0x7f;

    assert(z80_step(&cpu) == 11);

    assert(bus.memory[0x4000] == 0x80);

    assert(cpu.f & Z80_FLAG_S);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);
}

static void test_dec_memory(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * DEC (HL)
     */
    bus.memory[0] = 0x35;

    cpu.h = 0x44;
    cpu.l = 0x00;

    bus.memory[0x4400] = 0x01;

    assert(z80_step(&cpu) == 11);

    assert(bus.memory[0x4400] == 0x00);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_N);
}

int main(void) {
    test_inc_register();
    test_inc_overflow();
    test_inc_zero();

    test_dec_register();
    test_dec_overflow();

    test_inc_memory();
    test_dec_memory();

    puts("PASS z80 INC/DEC");

    return 0;
}
