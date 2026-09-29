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

static uint32_t run_immediate_alu(
    Z80 *cpu,
    TestBus *bus,
    uint8_t opcode,
    uint8_t operand
) {
    bus->memory[0] = opcode;
    bus->memory[1] = operand;

    cpu->pc = 0;

    return z80_step(cpu);
}

static void test_add(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0x7f;

    assert(
        run_immediate_alu(
            &cpu,
            &bus,
            0xc6,
            0x01
        ) == 7
    );

    assert(cpu.a == 0x80);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);
    assert(!(cpu.f & Z80_FLAG_N));
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_add_carry(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0xff;

    assert(
        run_immediate_alu(
            &cpu,
            &bus,
            0xc6,
            0x01
        ) == 7
    );

    assert(cpu.a == 0x00);

    assert(!(cpu.f & Z80_FLAG_S));
    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_H);
    assert(!(cpu.f & Z80_FLAG_PV));
    assert(!(cpu.f & Z80_FLAG_N));
    assert(cpu.f & Z80_FLAG_C);
}

static void test_sub(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0x80;

    assert(
        run_immediate_alu(
            &cpu,
            &bus,
            0xd6,
            0x01
        ) == 7
    );

    assert(cpu.a == 0x7f);

    assert(!(cpu.f & Z80_FLAG_S));
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);
    assert(cpu.f & Z80_FLAG_N);
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_sub_borrow(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0x00;

    assert(
        run_immediate_alu(
            &cpu,
            &bus,
            0xd6,
            0x01
        ) == 7
    );

    assert(cpu.a == 0xff);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_H);
    assert(!(cpu.f & Z80_FLAG_PV));
    assert(cpu.f & Z80_FLAG_N);
    assert(cpu.f & Z80_FLAG_C);
}

static void test_and(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0xf0;

    assert(
        run_immediate_alu(
            &cpu,
            &bus,
            0xe6,
            0x0f
        ) == 7
    );

    assert(cpu.a == 0x00);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);
    assert(!(cpu.f & Z80_FLAG_N));
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_xor(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0xaa;

    assert(
        run_immediate_alu(
            &cpu,
            &bus,
            0xee,
            0xaa
        ) == 7
    );

    assert(cpu.a == 0x00);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_PV);
    assert(!(cpu.f & Z80_FLAG_H));
    assert(!(cpu.f & Z80_FLAG_N));
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_or(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0x80;

    assert(
        run_immediate_alu(
            &cpu,
            &bus,
            0xf6,
            0x01
        ) == 7
    );

    assert(cpu.a == 0x81);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_PV);
    assert(!(cpu.f & Z80_FLAG_H));
    assert(!(cpu.f & Z80_FLAG_N));
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_cp(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0x42;

    assert(
        run_immediate_alu(
            &cpu,
            &bus,
            0xfe,
            0x42
        ) == 7
    );

    /*
     * CP performs subtraction only for flags.
     * A must not change.
     */
    assert(cpu.a == 0x42);

    assert(!(cpu.f & Z80_FLAG_S));
    assert(cpu.f & Z80_FLAG_Z);
    assert(!(cpu.f & Z80_FLAG_H));
    assert(!(cpu.f & Z80_FLAG_PV));
    assert(cpu.f & Z80_FLAG_N);
    assert(!(cpu.f & Z80_FLAG_C));
}

int main(void) {
    test_add();
    test_add_carry();

    test_sub();
    test_sub_borrow();

    test_and();
    test_xor();
    test_or();

    test_cp();

    puts("PASS z80 ALU");

    return 0;
}
