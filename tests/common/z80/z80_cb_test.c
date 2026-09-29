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

static void load_cb(
    TestBus *bus,
    uint8_t opcode
) {
    bus->memory[0] = 0xcb;
    bus->memory[1] = opcode;
}

static void test_register_rotates(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RLC B
     */
    load_cb(&bus, 0x00);

    cpu.b = 0x81;

    assert(z80_step(&cpu) == 8);

    assert(cpu.b == 0x03);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_PV);

    /*
     * RR C
     */
    cpu.pc = 0;
    load_cb(&bus, 0x19);

    cpu.c = 0x01;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 8);

    assert(cpu.c == 0x80);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_S);
}

static void test_shift_family(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * SLA D
     */
    load_cb(&bus, 0x22);

    cpu.d = 0x81;

    assert(z80_step(&cpu) == 8);

    assert(cpu.d == 0x02);
    assert(cpu.f & Z80_FLAG_C);

    /*
     * SRA E
     */
    cpu.pc = 0;
    load_cb(&bus, 0x2b);

    cpu.e = 0x81;

    assert(z80_step(&cpu) == 8);

    assert(cpu.e == 0xc0);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_S);

    /*
     * SLL H — undocumented Z80 instruction.
     */
    cpu.pc = 0;
    load_cb(&bus, 0x34);

    cpu.h = 0x80;

    assert(z80_step(&cpu) == 8);

    assert(cpu.h == 0x01);
    assert(cpu.f & Z80_FLAG_C);

    /*
     * SRL L
     */
    cpu.pc = 0;
    load_cb(&bus, 0x3d);

    cpu.l = 0x03;

    assert(z80_step(&cpu) == 8);

    assert(cpu.l == 0x01);
    assert(cpu.f & Z80_FLAG_C);
}

static void test_memory_rotate(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RLC (HL)
     */
    load_cb(&bus, 0x06);

    cpu.h = 0x40;
    cpu.l = 0x00;

    bus.memory[0x4000] = 0x80;

    assert(z80_step(&cpu) == 15);

    assert(bus.memory[0x4000] == 0x01);
    assert(cpu.f & Z80_FLAG_C);

    /*
     * Prefix + CB opcode are two M1 fetches.
     */
    assert(cpu.r == 2);
}

static void test_bit_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * BIT 7,A
     */
    load_cb(&bus, 0x7f);

    cpu.a = 0x80;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 8);

    assert(cpu.a == 0x80);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(!(cpu.f & Z80_FLAG_PV));

    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_C);
    assert(!(cpu.f & Z80_FLAG_N));

    /*
     * BIT 0,A with zero bit.
     */
    cpu.pc = 0;
    load_cb(&bus, 0x47);

    cpu.a = 0x80;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 8);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_PV);
    assert(cpu.f & Z80_FLAG_C);
}

static void test_bit_memory(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * BIT 3,(HL)
     */
    load_cb(&bus, 0x5e);

    cpu.h = 0x28;
    cpu.l = 0x00;

    bus.memory[0x2800] = 0x00;

    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 12);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_PV);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_C);

    /*
     * For BIT (HL), undocumented X/Y come from
     * the high byte of the address. 28h has both.
     */
    assert(cpu.f & Z80_FLAG_X);
    assert(cpu.f & Z80_FLAG_Y);
}

static void test_res_set_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RES 3,B
     */
    load_cb(&bus, 0x98);

    cpu.b = 0xff;
    cpu.f = 0xa5;

    assert(z80_step(&cpu) == 8);

    assert(cpu.b == 0xf7);

    /*
     * RES/SET do not modify flags.
     */
    assert(cpu.f == 0xa5);

    /*
     * SET 0,C
     */
    cpu.pc = 0;
    load_cb(&bus, 0xc1);

    cpu.c = 0x00;
    cpu.f = 0x5a;

    assert(z80_step(&cpu) == 8);

    assert(cpu.c == 0x01);
    assert(cpu.f == 0x5a);
}

static void test_res_set_memory(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.h = 0x44;
    cpu.l = 0x00;

    /*
     * RES 7,(HL)
     */
    load_cb(&bus, 0xbe);

    bus.memory[0x4400] = 0xff;
    cpu.f = 0xa5;

    assert(z80_step(&cpu) == 15);

    assert(bus.memory[0x4400] == 0x7f);
    assert(cpu.f == 0xa5);

    /*
     * SET 4,(HL)
     */
    cpu.pc = 0;
    load_cb(&bus, 0xe6);

    bus.memory[0x4400] = 0x00;
    cpu.f = 0x5a;

    assert(z80_step(&cpu) == 15);

    assert(bus.memory[0x4400] == 0x10);
    assert(cpu.f == 0x5a);
}

int main(void) {
    test_register_rotates();
    test_shift_family();
    test_memory_rotate();

    test_bit_register();
    test_bit_memory();

    test_res_set_register();
    test_res_set_memory();

    puts("PASS z80 CB instruction space");

    return 0;
}
