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

static void test_inc_dec_16(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    static const uint8_t program[] = {
        0x03, /* INC BC */
        0x13, /* INC DE */
        0x23, /* INC HL */
        0x33, /* INC SP */
        0x0b, /* DEC BC */
        0x1b, /* DEC DE */
        0x2b, /* DEC HL */
        0x3b  /* DEC SP */
    };

    for (
        uint32_t i = 0;
        i < sizeof(program);
        i++
    ) {
        bus.memory[i] = program[i];
    }

    cpu.b = 0x12;
    cpu.c = 0xff;

    cpu.d = 0xff;
    cpu.e = 0xff;

    cpu.h = 0x00;
    cpu.l = 0x00;

    cpu.sp = 0x8000;

    cpu.f = 0xa5;

    assert(z80_step(&cpu) == 6);
    assert(cpu.b == 0x13);
    assert(cpu.c == 0x00);

    assert(z80_step(&cpu) == 6);
    assert(cpu.d == 0x00);
    assert(cpu.e == 0x00);

    assert(z80_step(&cpu) == 6);
    assert(cpu.h == 0x00);
    assert(cpu.l == 0x01);

    assert(z80_step(&cpu) == 6);
    assert(cpu.sp == 0x8001);

    assert(z80_step(&cpu) == 6);
    assert(cpu.b == 0x12);
    assert(cpu.c == 0xff);

    assert(z80_step(&cpu) == 6);
    assert(cpu.d == 0xff);
    assert(cpu.e == 0xff);

    assert(z80_step(&cpu) == 6);
    assert(cpu.h == 0x00);
    assert(cpu.l == 0x00);

    assert(z80_step(&cpu) == 6);
    assert(cpu.sp == 0x8000);

    /*
     * 16-bit INC/DEC do not alter flags.
     */
    assert(cpu.f == 0xa5);
}

static void test_djnz(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0x10;
    bus.memory[1] = 0x02;

    cpu.b = 2;
    cpu.f = 0xa5;

    assert(z80_step(&cpu) == 13);
    assert(cpu.b == 1);
    assert(cpu.pc == 4);
    assert(cpu.f == 0xa5);

    cpu.pc = 0;
    cpu.b = 1;

    assert(z80_step(&cpu) == 8);
    assert(cpu.b == 0);
    assert(cpu.pc == 2);
    assert(cpu.f == 0xa5);
}

static void run_jr_case(
    uint8_t opcode,
    uint8_t flags,
    bool expected_taken
) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = opcode;
    bus.memory[1] = 0x05;

    cpu.f = flags;

    const uint32_t cycles =
        z80_step(&cpu);

    if (expected_taken) {
        assert(cycles == 12);
        assert(cpu.pc == 7);
    } else {
        assert(cycles == 7);
        assert(cpu.pc == 2);
    }

    /*
     * JR conditions do not alter flags.
     */
    assert(cpu.f == flags);
}

static void test_conditional_jr(void) {
    /*
     * NZ
     */
    run_jr_case(
        0x20,
        0,
        true
    );

    run_jr_case(
        0x20,
        Z80_FLAG_Z,
        false
    );

    /*
     * Z
     */
    run_jr_case(
        0x28,
        Z80_FLAG_Z,
        true
    );

    run_jr_case(
        0x28,
        0,
        false
    );

    /*
     * NC
     */
    run_jr_case(
        0x30,
        0,
        true
    );

    run_jr_case(
        0x30,
        Z80_FLAG_C,
        false
    );

    /*
     * C
     */
    run_jr_case(
        0x38,
        Z80_FLAG_C,
        true
    );

    run_jr_case(
        0x38,
        0,
        false
    );
}

int main(void) {
    test_inc_dec_16();
    test_djnz();
    test_conditional_jr();

    puts("PASS z80 16-bit INC/DEC and relative branches");

    return 0;
}
