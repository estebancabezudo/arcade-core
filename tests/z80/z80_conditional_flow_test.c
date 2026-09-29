#include <assert.h>
#include <stdbool.h>
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

static void test_add_hl(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0x09; /* ADD HL,BC */

    cpu.h = 0x0f;
    cpu.l = 0xff;

    cpu.b = 0x00;
    cpu.c = 0x01;

    cpu.f =
        Z80_FLAG_S |
        Z80_FLAG_Z |
        Z80_FLAG_PV |
        Z80_FLAG_N |
        Z80_FLAG_C;

    assert(z80_step(&cpu) == 11);

    assert(cpu.h == 0x10);
    assert(cpu.l == 0x00);

    assert(cpu.f & Z80_FLAG_S);
    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_PV);

    assert(cpu.f & Z80_FLAG_H);

    assert(!(cpu.f & Z80_FLAG_N));
    assert(!(cpu.f & Z80_FLAG_C));

    cpu.pc = 0;
    bus.memory[0] = 0x19; /* ADD HL,DE */

    cpu.h = 0xff;
    cpu.l = 0xff;

    cpu.d = 0x00;
    cpu.e = 0x01;

    cpu.f = 0;

    assert(z80_step(&cpu) == 11);

    assert(cpu.h == 0x00);
    assert(cpu.l == 0x00);

    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_C);
}

static void run_jp(
    uint8_t opcode,
    uint8_t flags,
    bool taken
) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = opcode;
    bus.memory[1] = 0x34;
    bus.memory[2] = 0x12;

    cpu.f = flags;

    assert(z80_step(&cpu) == 10);

    assert(
        cpu.pc ==
        (taken ? 0x1234 : 0x0003)
    );
}

static void test_jp_conditions(void) {
    run_jp(0xc2, 0, true);           /* NZ */
    run_jp(0xca, Z80_FLAG_Z, true);  /* Z  */

    run_jp(0xd2, 0, true);           /* NC */
    run_jp(0xda, Z80_FLAG_C, true);  /* C  */

    run_jp(0xe2, 0, true);           /* PO */
    run_jp(0xea, Z80_FLAG_PV, true); /* PE */

    run_jp(0xf2, 0, true);           /* P  */
    run_jp(0xfa, Z80_FLAG_S, true);  /* M  */

    run_jp(0xc2, Z80_FLAG_Z, false);
    run_jp(0xda, 0, false);
    run_jp(0xea, 0, false);
    run_jp(0xfa, 0, false);
}

static void test_call_condition(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * CALL Z,1234h
     */
    bus.memory[0] = 0xcc;
    bus.memory[1] = 0x34;
    bus.memory[2] = 0x12;

    cpu.f = Z80_FLAG_Z;
    cpu.sp = 0x9000;

    assert(z80_step(&cpu) == 17);

    assert(cpu.pc == 0x1234);
    assert(cpu.sp == 0x8ffe);

    assert(bus.memory[0x8fff] == 0x00);
    assert(bus.memory[0x8ffe] == 0x03);

    /*
     * CALL NZ,5678h — not taken.
     */
    cpu.pc = 0x0100;
    cpu.sp = 0x9000;
    cpu.f = Z80_FLAG_Z;

    bus.memory[0x0100] = 0xc4;
    bus.memory[0x0101] = 0x78;
    bus.memory[0x0102] = 0x56;

    assert(z80_step(&cpu) == 10);

    assert(cpu.pc == 0x0103);
    assert(cpu.sp == 0x9000);
}

static void test_ret_condition(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RET C
     */
    bus.memory[0] = 0xd8;

    cpu.f = Z80_FLAG_C;
    cpu.sp = 0x8000;

    bus.memory[0x8000] = 0x78;
    bus.memory[0x8001] = 0x56;

    assert(z80_step(&cpu) == 11);

    assert(cpu.pc == 0x5678);
    assert(cpu.sp == 0x8002);

    /*
     * RET NC — not taken.
     */
    cpu.pc = 0x0100;
    cpu.sp = 0x9000;
    cpu.f = Z80_FLAG_C;

    bus.memory[0x0100] = 0xd0;

    assert(z80_step(&cpu) == 5);

    assert(cpu.pc == 0x0101);
    assert(cpu.sp == 0x9000);
}

static void test_rst(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RST 38h
     */
    bus.memory[0] = 0xff;

    cpu.sp = 0x9000;

    assert(z80_step(&cpu) == 11);

    assert(cpu.pc == 0x0038);
    assert(cpu.sp == 0x8ffe);

    assert(bus.memory[0x8fff] == 0x00);
    assert(bus.memory[0x8ffe] == 0x01);
}

int main(void) {
    test_add_hl();

    test_jp_conditions();
    test_call_condition();
    test_ret_condition();

    test_rst();

    puts(
        "PASS z80 ADD HL, conditional flow and RST"
    );

    return 0;
}
