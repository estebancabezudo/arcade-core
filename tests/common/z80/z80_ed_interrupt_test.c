#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];
    uint32_t reti_count;
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

static void reti_callback(
    void *context
) {
    TestBus *bus = context;

    bus->reti_count++;
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
        .reti = reti_callback
    };

    z80_init(
        &cpu,
        &bus
    );

    return cpu;
}

static void load_ed(
    TestBus *bus,
    uint8_t opcode
) {
    bus->memory[0] = 0xed;
    bus->memory[1] = opcode;
}

static void test_neg(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * NEG 01h -> FFh
     */
    load_ed(
        &bus,
        0x44
    );

    cpu.a = 0x01;

    assert(z80_step(&cpu) == 8);

    assert(cpu.a == 0xff);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));

    assert(cpu.f & Z80_FLAG_H);
    assert(!(cpu.f & Z80_FLAG_PV));

    assert(cpu.f & Z80_FLAG_N);
    assert(cpu.f & Z80_FLAG_C);

    /*
     * NEG 80h -> 80h with overflow.
     *
     * Use an alias opcode as well.
     */
    cpu.pc = 0;

    load_ed(
        &bus,
        0x4c
    );

    cpu.a = 0x80;

    assert(z80_step(&cpu) == 8);

    assert(cpu.a == 0x80);

    assert(cpu.f & Z80_FLAG_S);
    assert(cpu.f & Z80_FLAG_PV);
    assert(cpu.f & Z80_FLAG_N);
    assert(cpu.f & Z80_FLAG_C);

    /*
     * NEG 00h -> 00h with no carry.
     */
    cpu.pc = 0;

    load_ed(
        &bus,
        0x44
    );

    cpu.a = 0x00;

    assert(z80_step(&cpu) == 8);

    assert(cpu.a == 0x00);
    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_N);

    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_retn(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RETN
     */
    load_ed(
        &bus,
        0x45
    );

    cpu.sp = 0x8000;

    bus.memory[0x8000] = 0x34;
    bus.memory[0x8001] = 0x12;

    cpu.iff1 = false;
    cpu.iff2 = true;

    assert(z80_step(&cpu) == 14);

    assert(cpu.pc == 0x1234);
    assert(cpu.sp == 0x8002);

    assert(cpu.iff1);
    assert(cpu.iff2);

    assert(bus.reti_count == 0);
}

static void test_reti(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RETI
     */
    load_ed(
        &bus,
        0x4d
    );

    cpu.sp = 0x9000;

    bus.memory[0x9000] = 0x78;
    bus.memory[0x9001] = 0x56;

    cpu.iff1 = false;
    cpu.iff2 = true;

    assert(z80_step(&cpu) == 14);

    assert(cpu.pc == 0x5678);
    assert(cpu.sp == 0x9002);

    assert(cpu.iff1);
    assert(cpu.iff2);

    assert(bus.reti_count == 1);
}

static void test_interrupt_modes(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0x56
    );

    assert(z80_step(&cpu) == 8);
    assert(cpu.interrupt_mode == 1);

    cpu.pc = 0;

    load_ed(
        &bus,
        0x5e
    );

    assert(z80_step(&cpu) == 8);
    assert(cpu.interrupt_mode == 2);

    cpu.pc = 0;

    load_ed(
        &bus,
        0x46
    );

    assert(z80_step(&cpu) == 8);
    assert(cpu.interrupt_mode == 0);

    /*
     * Test documented aliases too.
     */
    cpu.pc = 0;

    load_ed(
        &bus,
        0x76
    );

    assert(z80_step(&cpu) == 8);
    assert(cpu.interrupt_mode == 1);

    cpu.pc = 0;

    load_ed(
        &bus,
        0x7e
    );

    assert(z80_step(&cpu) == 8);
    assert(cpu.interrupt_mode == 2);
}

static void test_ld_i_a(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0x47
    );

    cpu.a = 0x5a;

    assert(z80_step(&cpu) == 9);

    assert(cpu.i == 0x5a);
}

static void test_ld_r_a(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0x4f
    );

    cpu.a = 0xa5;

    assert(z80_step(&cpu) == 9);

    /*
     * LD R,A occurs after both opcode fetches.
     * The final write replaces the incremented R value.
     */
    assert(cpu.r == 0xa5);
}

static void test_ld_a_i(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0x57
    );

    cpu.i = 0x80;
    cpu.iff2 = true;

    cpu.f =
        Z80_FLAG_C |
        Z80_FLAG_H |
        Z80_FLAG_N;

    assert(z80_step(&cpu) == 9);

    assert(cpu.a == 0x80);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));

    assert(cpu.f & Z80_FLAG_PV);
    assert(cpu.f & Z80_FLAG_C);

    assert(!(cpu.f & Z80_FLAG_H));
    assert(!(cpu.f & Z80_FLAG_N));
}

static void test_ld_a_r(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(
        &bus,
        0x5f
    );

    cpu.r = 0x00;
    cpu.iff2 = false;

    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 9);

    /*
     * ED and 5F are both M1 fetches, therefore R has
     * advanced from 00 to 02 before LD A,R executes.
     */
    assert(cpu.a == 0x02);

    assert(!(cpu.f & Z80_FLAG_PV));
    assert(cpu.f & Z80_FLAG_C);
}

int main(void) {
    test_neg();

    test_retn();
    test_reti();

    test_interrupt_modes();

    test_ld_i_a();
    test_ld_r_a();

    test_ld_a_i();
    test_ld_a_r();

    puts(
        "PASS z80 ED NEG, interrupt control and I/R transfers"
    );

    return 0;
}
