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

static void load_ed(
    TestBus *bus,
    uint8_t opcode
) {
    bus->memory[0] = 0xed;
    bus->memory[1] = opcode;
}

static uint16_t bc(
    const Z80 *cpu
) {
    return (uint16_t)(
        ((uint16_t)cpu->b << 8) |
        cpu->c
    );
}

static uint16_t de(
    const Z80 *cpu
) {
    return (uint16_t)(
        ((uint16_t)cpu->d << 8) |
        cpu->e
    );
}

static uint16_t hl(
    const Z80 *cpu
) {
    return (uint16_t)(
        ((uint16_t)cpu->h << 8) |
        cpu->l
    );
}

static void test_ldi(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0xa0
    );

    cpu.a = 0x10;

    cpu.h = 0x40;
    cpu.l = 0x00;

    cpu.d = 0x50;
    cpu.e = 0x00;

    cpu.b = 0x00;
    cpu.c = 0x02;

    cpu.f =
        Z80_FLAG_S |
        Z80_FLAG_Z |
        Z80_FLAG_C |
        Z80_FLAG_H |
        Z80_FLAG_N;

    bus.memory[0x4000] = 0x22;

    assert(z80_step(&cpu) == 16);

    assert(bus.memory[0x5000] == 0x22);

    assert(hl(&cpu) == 0x4001);
    assert(de(&cpu) == 0x5001);
    assert(bc(&cpu) == 0x0001);

    assert(cpu.f & Z80_FLAG_S);
    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_C);

    assert(!(cpu.f & Z80_FLAG_H));
    assert(!(cpu.f & Z80_FLAG_N));

    assert(cpu.f & Z80_FLAG_PV);
}

static void test_ldd(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0xa8
    );

    cpu.h = 0x40;
    cpu.l = 0x02;

    cpu.d = 0x50;
    cpu.e = 0x02;

    cpu.b = 0x00;
    cpu.c = 0x01;

    bus.memory[0x4002] = 0x99;

    assert(z80_step(&cpu) == 16);

    assert(bus.memory[0x5002] == 0x99);

    assert(hl(&cpu) == 0x4001);
    assert(de(&cpu) == 0x5001);
    assert(bc(&cpu) == 0x0000);

    assert(!(cpu.f & Z80_FLAG_PV));
}

static void test_ldir(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0xb0
    );

    cpu.h = 0x40;
    cpu.l = 0x00;

    cpu.d = 0x50;
    cpu.e = 0x00;

    cpu.b = 0x00;
    cpu.c = 0x02;

    bus.memory[0x4000] = 0x11;
    bus.memory[0x4001] = 0x22;

    /*
     * First iteration repeats.
     */
    assert(z80_step(&cpu) == 21);

    assert(cpu.pc == 0x0000);
    assert(bus.memory[0x5000] == 0x11);
    assert(bc(&cpu) == 0x0001);

    /*
     * Second iteration terminates.
     */
    assert(z80_step(&cpu) == 16);

    assert(cpu.pc == 0x0002);

    assert(bus.memory[0x5001] == 0x22);

    assert(hl(&cpu) == 0x4002);
    assert(de(&cpu) == 0x5002);
    assert(bc(&cpu) == 0x0000);

    assert(!(cpu.f & Z80_FLAG_PV));

    /*
     * Two M1 fetches per iteration.
     */
    assert(cpu.r == 4);
}

static void test_lddr(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0xb8
    );

    cpu.h = 0x40;
    cpu.l = 0x01;

    cpu.d = 0x50;
    cpu.e = 0x01;

    cpu.b = 0x00;
    cpu.c = 0x02;

    bus.memory[0x4001] = 0xaa;
    bus.memory[0x4000] = 0xbb;

    assert(z80_step(&cpu) == 21);
    assert(z80_step(&cpu) == 16);

    assert(bus.memory[0x5001] == 0xaa);
    assert(bus.memory[0x5000] == 0xbb);

    assert(hl(&cpu) == 0x3fff);
    assert(de(&cpu) == 0x4fff);
    assert(bc(&cpu) == 0);
}

static void test_cpi(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0xa1
    );

    cpu.a = 0x20;

    cpu.h = 0x40;
    cpu.l = 0x00;

    cpu.b = 0x00;
    cpu.c = 0x02;

    cpu.f = Z80_FLAG_C;

    bus.memory[0x4000] = 0x10;

    assert(z80_step(&cpu) == 16);

    assert(hl(&cpu) == 0x4001);
    assert(bc(&cpu) == 0x0001);

    assert(!(cpu.f & Z80_FLAG_Z));
    assert(cpu.f & Z80_FLAG_N);
    assert(cpu.f & Z80_FLAG_PV);

    /*
     * Carry is preserved.
     */
    assert(cpu.f & Z80_FLAG_C);
}

static void test_cpd(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0xa9
    );

    cpu.a = 0x42;

    cpu.h = 0x40;
    cpu.l = 0x02;

    cpu.b = 0x00;
    cpu.c = 0x01;

    bus.memory[0x4002] = 0x42;

    assert(z80_step(&cpu) == 16);

    assert(hl(&cpu) == 0x4001);
    assert(bc(&cpu) == 0);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_N);
    assert(!(cpu.f & Z80_FLAG_PV));
}

static void test_cpir_stops_on_match(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0xb1
    );

    cpu.a = 0x33;

    cpu.h = 0x40;
    cpu.l = 0x00;

    cpu.b = 0x00;
    cpu.c = 0x03;

    bus.memory[0x4000] = 0x11;
    bus.memory[0x4001] = 0x33;
    bus.memory[0x4002] = 0x55;

    /*
     * First comparison: no match, repeat.
     */
    assert(z80_step(&cpu) == 21);

    assert(cpu.pc == 0);
    assert(hl(&cpu) == 0x4001);
    assert(bc(&cpu) == 2);

    /*
     * Second comparison matches and terminates even
     * though BC remains nonzero.
     */
    assert(z80_step(&cpu) == 16);

    assert(cpu.pc == 2);
    assert(hl(&cpu) == 0x4002);
    assert(bc(&cpu) == 1);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_PV);
}

static void test_cpdr_exhausts_count(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0xb9
    );

    cpu.a = 0x99;

    cpu.h = 0x40;
    cpu.l = 0x01;

    cpu.b = 0x00;
    cpu.c = 0x02;

    bus.memory[0x4001] = 0x10;
    bus.memory[0x4000] = 0x20;

    assert(z80_step(&cpu) == 21);
    assert(z80_step(&cpu) == 16);

    assert(hl(&cpu) == 0x3fff);
    assert(bc(&cpu) == 0);

    assert(!(cpu.f & Z80_FLAG_Z));
    assert(!(cpu.f & Z80_FLAG_PV));
}

int main(void) {
    test_ldi();
    test_ldd();

    test_ldir();
    test_lddr();

    test_cpi();
    test_cpd();

    test_cpir_stops_on_match();
    test_cpdr_exhausts_count();

    puts(
        "PASS z80 ED memory block transfer and compare"
    );

    return 0;
}
