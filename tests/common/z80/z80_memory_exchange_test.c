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

static void test_bc_de_indirect(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * LD (BC),A
     */
    bus.memory[0] = 0x02;

    cpu.b = 0x40;
    cpu.c = 0x10;
    cpu.a = 0xa5;

    assert(z80_step(&cpu) == 7);
    assert(bus.memory[0x4010] == 0xa5);

    /*
     * LD A,(BC)
     */
    cpu.pc = 0;
    bus.memory[0] = 0x0a;
    bus.memory[0x4010] = 0x5a;
    cpu.a = 0;

    assert(z80_step(&cpu) == 7);
    assert(cpu.a == 0x5a);

    /*
     * LD (DE),A
     */
    cpu.pc = 0;
    bus.memory[0] = 0x12;
    cpu.d = 0x44;
    cpu.e = 0x20;
    cpu.a = 0x77;

    assert(z80_step(&cpu) == 7);
    assert(bus.memory[0x4420] == 0x77);

    /*
     * LD A,(DE)
     */
    cpu.pc = 0;
    bus.memory[0] = 0x1a;
    bus.memory[0x4420] = 0x99;
    cpu.a = 0;

    assert(z80_step(&cpu) == 7);
    assert(cpu.a == 0x99);
}

static void test_absolute_memory(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * LD (1234h),HL
     */
    bus.memory[0] = 0x22;
    bus.memory[1] = 0x34;
    bus.memory[2] = 0x12;

    cpu.h = 0xab;
    cpu.l = 0xcd;

    assert(z80_step(&cpu) == 16);

    assert(bus.memory[0x1234] == 0xcd);
    assert(bus.memory[0x1235] == 0xab);

    /*
     * LD HL,(1234h)
     */
    cpu.pc = 0;
    bus.memory[0] = 0x2a;
    bus.memory[1] = 0x34;
    bus.memory[2] = 0x12;

    bus.memory[0x1234] = 0x78;
    bus.memory[0x1235] = 0x56;

    cpu.h = 0;
    cpu.l = 0;

    assert(z80_step(&cpu) == 16);

    assert(cpu.h == 0x56);
    assert(cpu.l == 0x78);

    /*
     * LD (2000h),A
     */
    cpu.pc = 0;
    bus.memory[0] = 0x32;
    bus.memory[1] = 0x00;
    bus.memory[2] = 0x20;

    cpu.a = 0x42;

    assert(z80_step(&cpu) == 13);
    assert(bus.memory[0x2000] == 0x42);

    /*
     * LD A,(2000h)
     */
    cpu.pc = 0;
    bus.memory[0] = 0x3a;
    bus.memory[1] = 0x00;
    bus.memory[2] = 0x20;

    bus.memory[0x2000] = 0xe7;
    cpu.a = 0;

    assert(z80_step(&cpu) == 13);
    assert(cpu.a == 0xe7);
}

static void test_exchange_registers(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * EX AF,AF'
     */
    bus.memory[0] = 0x08;

    cpu.a = 0x11;
    cpu.f = 0x22;

    cpu.a_alt = 0xaa;
    cpu.f_alt = 0xbb;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0xaa);
    assert(cpu.f == 0xbb);

    assert(cpu.a_alt == 0x11);
    assert(cpu.f_alt == 0x22);

    /*
     * EXX
     */
    cpu.pc = 0;
    bus.memory[0] = 0xd9;

    cpu.b = 0x01;
    cpu.c = 0x02;
    cpu.d = 0x03;
    cpu.e = 0x04;
    cpu.h = 0x05;
    cpu.l = 0x06;

    cpu.b_alt = 0xa1;
    cpu.c_alt = 0xa2;
    cpu.d_alt = 0xa3;
    cpu.e_alt = 0xa4;
    cpu.h_alt = 0xa5;
    cpu.l_alt = 0xa6;

    assert(z80_step(&cpu) == 4);

    assert(cpu.b == 0xa1);
    assert(cpu.c == 0xa2);
    assert(cpu.d == 0xa3);
    assert(cpu.e == 0xa4);
    assert(cpu.h == 0xa5);
    assert(cpu.l == 0xa6);

    assert(cpu.b_alt == 0x01);
    assert(cpu.c_alt == 0x02);
    assert(cpu.d_alt == 0x03);
    assert(cpu.e_alt == 0x04);
    assert(cpu.h_alt == 0x05);
    assert(cpu.l_alt == 0x06);

    /*
     * EX DE,HL
     */
    cpu.pc = 0;
    bus.memory[0] = 0xeb;

    cpu.d = 0x12;
    cpu.e = 0x34;

    cpu.h = 0x56;
    cpu.l = 0x78;

    assert(z80_step(&cpu) == 4);

    assert(cpu.d == 0x56);
    assert(cpu.e == 0x78);

    assert(cpu.h == 0x12);
    assert(cpu.l == 0x34);
}

static void test_exchange_stack(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * EX (SP),HL
     */
    bus.memory[0] = 0xe3;

    cpu.sp = 0x8000;

    cpu.h = 0x12;
    cpu.l = 0x34;

    bus.memory[0x8000] = 0x78;
    bus.memory[0x8001] = 0x56;

    assert(z80_step(&cpu) == 19);

    assert(cpu.h == 0x56);
    assert(cpu.l == 0x78);

    assert(bus.memory[0x8000] == 0x34);
    assert(bus.memory[0x8001] == 0x12);
}

static void test_hl_indirect_control(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * JP (HL)
     */
    bus.memory[0] = 0xe9;

    cpu.h = 0x12;
    cpu.l = 0x34;

    assert(z80_step(&cpu) == 4);
    assert(cpu.pc == 0x1234);

    /*
     * LD SP,HL
     */
    cpu.pc = 0;
    bus.memory[0] = 0xf9;

    cpu.h = 0xab;
    cpu.l = 0xcd;
    cpu.sp = 0;

    assert(z80_step(&cpu) == 6);
    assert(cpu.sp == 0xabcd);
}

int main(void) {
    test_bc_de_indirect();
    test_absolute_memory();

    test_exchange_registers();
    test_exchange_stack();

    test_hl_indirect_control();

    puts(
        "PASS z80 memory loads, exchanges and HL indirect"
    );

    return 0;
}
