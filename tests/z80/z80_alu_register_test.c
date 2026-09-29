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

static void test_add_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * ADD A,B
     */
    bus.memory[0] = 0x80;

    cpu.a = 0x7f;
    cpu.b = 0x01;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x80);
    assert(cpu.f & Z80_FLAG_S);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_adc_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * ADC A,C
     */
    bus.memory[0] = 0x89;

    cpu.a = 0x7f;
    cpu.c = 0x00;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x80);
    assert(cpu.f & Z80_FLAG_S);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);
    assert(!(cpu.f & Z80_FLAG_N));
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_sub_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * SUB D
     */
    bus.memory[0] = 0x92;

    cpu.a = 0x10;
    cpu.d = 0x01;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x0f);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_N);
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_sbc_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * SBC A,E
     */
    bus.memory[0] = 0x9b;

    cpu.a = 0x00;
    cpu.e = 0x00;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0xff);
    assert(cpu.f & Z80_FLAG_S);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_N);
    assert(cpu.f & Z80_FLAG_C);
}

static void test_logical_registers(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * AND H
     */
    bus.memory[0] = 0xa4;

    cpu.a = 0xf3;
    cpu.h = 0x0f;

    assert(z80_step(&cpu) == 4);
    assert(cpu.a == 0x03);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);

    /*
     * XOR L
     */
    cpu.pc = 0;
    bus.memory[0] = 0xad;

    cpu.a = 0xaa;
    cpu.l = 0xff;

    assert(z80_step(&cpu) == 4);
    assert(cpu.a == 0x55);
    assert(cpu.f & Z80_FLAG_PV);

    /*
     * OR B
     */
    cpu.pc = 0;
    bus.memory[0] = 0xb0;

    cpu.a = 0x80;
    cpu.b = 0x01;

    assert(z80_step(&cpu) == 4);
    assert(cpu.a == 0x81);
    assert(cpu.f & Z80_FLAG_S);
    assert(cpu.f & Z80_FLAG_PV);
}

static void test_cp_register(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * CP C
     */
    bus.memory[0] = 0xb9;

    cpu.a = 0x42;
    cpu.c = 0x42;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x42);
    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_N);
}

static void test_alu_memory(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * ADD A,(HL)
     */
    bus.memory[0] = 0x86;

    cpu.h = 0x40;
    cpu.l = 0x00;

    cpu.a = 0x10;
    bus.memory[0x4000] = 0x20;

    assert(z80_step(&cpu) == 7);
    assert(cpu.a == 0x30);
}

int main(void) {
    test_add_register();
    test_adc_register();

    test_sub_register();
    test_sbc_register();

    test_logical_registers();
    test_cp_register();

    test_alu_memory();

    puts("PASS z80 register ALU");

    return 0;
}
