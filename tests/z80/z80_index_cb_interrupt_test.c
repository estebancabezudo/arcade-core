#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];
    uint8_t interrupt_vector;
    uint32_t interrupt_ack_count;
} TestBus;

static uint8_t memory_read(void *context, uint16_t address) {
    TestBus *bus = context;
    return bus->memory[address];
}

static void memory_write(void *context, uint16_t address, uint8_t value) {
    TestBus *bus = context;
    bus->memory[address] = value;
}

static uint8_t interrupt_acknowledge(void *context) {
    TestBus *bus = context;
    bus->interrupt_ack_count++;
    return bus->interrupt_vector;
}

static Z80 create_cpu(TestBus *test_bus) {
    Z80 cpu;

    const Z80Bus bus = {
        .context = test_bus,
        .memory_read = memory_read,
        .memory_write = memory_write,
        .io_read = 0,
        .io_write = 0,
        .interrupt_acknowledge = interrupt_acknowledge,
        .reti = 0
    };

    z80_init(&cpu, &bus);
    return cpu;
}

static void test_ddcb_rotate_copy(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /* DD CB +1 00 = RLC (IX+1),B */
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0xcb;
    bus.memory[2] = 0x01;
    bus.memory[3] = 0x00;

    cpu.ix = 0x4000;
    cpu.b = 0;
    bus.memory[0x4001] = 0x80;

    assert(z80_step(&cpu) == 23);
    assert(bus.memory[0x4001] == 0x01);
    assert(cpu.b == 0x01);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.pc == 4);
    assert(cpu.r == 2);
}

static void test_fdcb_bit(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /* FD CB -2 5E = BIT 3,(IY-2) */
    bus.memory[0] = 0xfd;
    bus.memory[1] = 0xcb;
    bus.memory[2] = 0xfe;
    bus.memory[3] = 0x5e;

    cpu.iy = 0x2802;
    cpu.f = Z80_FLAG_C;
    bus.memory[0x2800] = 0x00;

    assert(z80_step(&cpu) == 20);
    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_PV);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_X);
    assert(cpu.f & Z80_FLAG_Y);
    assert(cpu.r == 2);
}

static void test_ddcb_res_copy(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /* DD CB +0 BF = RES 7,(IX),A */
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0xcb;
    bus.memory[2] = 0x00;
    bus.memory[3] = 0xbf;

    cpu.ix = 0x4100;
    cpu.a = 0;
    cpu.f = 0xa5;
    bus.memory[0x4100] = 0xff;

    assert(z80_step(&cpu) == 23);
    assert(bus.memory[0x4100] == 0x7f);
    assert(cpu.a == 0x7f);
    assert(cpu.f == 0xa5);
}

static void test_ignored_prefix(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /* DD NOP */
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x00;

    assert(z80_step(&cpu) == 8);
    assert(cpu.pc == 2);
    assert(cpu.r == 2);

    /* DD EX DE,HL -- DD is ignored for this instruction. */
    z80_reset(&cpu);
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0xeb;

    cpu.d = 0x12;
    cpu.e = 0x34;
    cpu.h = 0x56;
    cpu.l = 0x78;

    assert(z80_step(&cpu) == 8);
    assert(cpu.d == 0x56);
    assert(cpu.e == 0x78);
    assert(cpu.h == 0x12);
    assert(cpu.l == 0x34);
}

static void test_repeated_prefix_last_wins(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /* DD FD LD IY,1234h. First DD is ignored; last index prefix wins. */
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0xfd;
    bus.memory[2] = 0x21;
    bus.memory[3] = 0x34;
    bus.memory[4] = 0x12;

    assert(z80_step(&cpu) == 18);
    assert(cpu.iy == 0x1234);
    assert(cpu.ix == 0x0000);
    assert(cpu.pc == 5);
    assert(cpu.r == 3);
}

static void test_index_before_ed_is_ignored(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /* DD ED 44 = ignored DD + NEG. */
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0xed;
    bus.memory[2] = 0x44;

    cpu.a = 0x01;

    assert(z80_step(&cpu) == 12);
    assert(cpu.a == 0xff);
    assert(cpu.pc == 3);
    assert(cpu.r == 3);
}

static void test_im0_rst(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.pc = 0x1234;
    cpu.sp = 0x9000;
    cpu.interrupt_mode = 0;
    cpu.iff1 = true;
    cpu.iff2 = true;

    bus.interrupt_vector = 0xcf; /* RST 08h */

    z80_set_irq(&cpu, true);

    assert(z80_step(&cpu) == 13);
    assert(bus.interrupt_ack_count == 1);
    assert(cpu.pc == 0x0008);
    assert(cpu.sp == 0x8ffe);
    assert(bus.memory[0x8ffe] == 0x34);
    assert(bus.memory[0x8fff] == 0x12);
    assert(!cpu.iff1);
    assert(!cpu.iff2);
    assert(cpu.r == 1);
}

static void test_im0_multibyte_instruction(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.pc = 0x2000;
    cpu.interrupt_mode = 0;
    cpu.iff1 = true;
    cpu.iff2 = true;

    /* Device supplies JP nn. Operand bytes come from current PC. */
    bus.interrupt_vector = 0xc3;
    bus.memory[0x2000] = 0x78;
    bus.memory[0x2001] = 0x56;

    z80_set_irq(&cpu, true);

    assert(z80_step(&cpu) == 12);
    assert(bus.interrupt_ack_count == 1);
    assert(cpu.pc == 0x5678);
    assert(cpu.r == 1);
}

int main(void) {
    test_ddcb_rotate_copy();
    test_fdcb_bit();
    test_ddcb_res_copy();

    test_ignored_prefix();
    test_repeated_prefix_last_wins();
    test_index_before_ed_is_ignored();

    test_im0_rst();
    test_im0_multibyte_instruction();

    puts("PASS z80 DDCB/FDCB, ignored prefixes and IM0");
    return 0;
}
