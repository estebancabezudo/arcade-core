#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];
} TestBus;

static uint8_t memory_read(void *context, uint16_t address) {
    TestBus *bus = context;
    return bus->memory[address];
}

static void memory_write(void *context, uint16_t address, uint8_t value) {
    TestBus *bus = context;
    bus->memory[address] = value;
}

static Z80 create_cpu(TestBus *test_bus) {
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

static void test_index_16bit(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x21;
    bus.memory[2] = 0x34;
    bus.memory[3] = 0x12;
    assert(z80_step(&cpu) == 14);
    assert(cpu.ix == 0x1234);

    cpu.pc = 0;
    bus.memory[0] = 0xfd;
    bus.memory[1] = 0x21;
    bus.memory[2] = 0x78;
    bus.memory[3] = 0x56;
    assert(z80_step(&cpu) == 14);
    assert(cpu.iy == 0x5678);

    cpu.pc = 0;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x09; /* ADD IX,BC */
    cpu.ix = 0x0fff;
    cpu.b = 0;
    cpu.c = 1;
    assert(z80_step(&cpu) == 15);
    assert(cpu.ix == 0x1000);
    assert(cpu.f & Z80_FLAG_H);
}

static void test_index_high_low(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.ix = 0x00aa;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x26; /* LD IXH,n */
    bus.memory[2] = 0x12;
    assert(z80_step(&cpu) == 11);
    assert(cpu.ix == 0x12aa);

    cpu.pc = 0;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x2e; /* LD IXL,n */
    bus.memory[2] = 0x34;
    assert(z80_step(&cpu) == 11);
    assert(cpu.ix == 0x1234);

    cpu.pc = 0;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x44; /* LD B,IXH */
    assert(z80_step(&cpu) == 8);
    assert(cpu.b == 0x12);
}

static void test_index_displacement(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.ix = 0x4000;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x36; /* LD (IX+5),42h */
    bus.memory[2] = 0x05;
    bus.memory[3] = 0x42;
    assert(z80_step(&cpu) == 19);
    assert(bus.memory[0x4005] == 0x42);

    cpu.pc = 0;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x7e; /* LD A,(IX+5) */
    bus.memory[2] = 0x05;
    assert(z80_step(&cpu) == 19);
    assert(cpu.a == 0x42);

    cpu.pc = 0;
    cpu.iy = 0x5000;
    bus.memory[0] = 0xfd;
    bus.memory[1] = 0x7e; /* LD A,(IY-2) */
    bus.memory[2] = 0xfe;
    bus.memory[0x4ffe] = 0x77;
    assert(z80_step(&cpu) == 19);
    assert(cpu.a == 0x77);
}

static void test_index_memory_h_l_rule(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.ix = 0xab00;
    cpu.h = 0x12;
    cpu.l = 0x34;

    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x66; /* LD H,(IX+1): real H */
    bus.memory[2] = 0x01;
    bus.memory[0xab01] = 0x55;
    assert(z80_step(&cpu) == 19);
    assert(cpu.h == 0x55);
    assert(cpu.ix == 0xab00);

    cpu.pc = 0;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x75; /* LD (IX+2),L: real L */
    bus.memory[2] = 0x02;
    cpu.l = 0x66;
    assert(z80_step(&cpu) == 19);
    assert(bus.memory[0xab02] == 0x66);
}

static void test_index_inc_alu(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.ix = 0x4000;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x34; /* INC (IX+1) */
    bus.memory[2] = 0x01;
    bus.memory[0x4001] = 0x7f;
    assert(z80_step(&cpu) == 23);
    assert(bus.memory[0x4001] == 0x80);
    assert(cpu.f & Z80_FLAG_PV);

    cpu.pc = 0;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0x86; /* ADD A,(IX+3) */
    bus.memory[2] = 0x03;
    bus.memory[0x4003] = 0x20;
    cpu.a = 0x10;
    assert(z80_step(&cpu) == 19);
    assert(cpu.a == 0x30);
}

static void test_index_stack_control(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0xdd;
    bus.memory[1] = 0xe5; /* PUSH IX */
    cpu.ix = 0x1234;
    cpu.sp = 0x9000;
    assert(z80_step(&cpu) == 15);
    assert(cpu.sp == 0x8ffe);
    assert(bus.memory[0x8ffe] == 0x34);
    assert(bus.memory[0x8fff] == 0x12);

    cpu.pc = 0;
    bus.memory[0] = 0xfd;
    bus.memory[1] = 0xe1; /* POP IY */
    assert(z80_step(&cpu) == 14);
    assert(cpu.iy == 0x1234);

    cpu.pc = 0;
    bus.memory[0] = 0xdd;
    bus.memory[1] = 0xe9; /* JP (IX) */
    cpu.ix = 0x4567;
    assert(z80_step(&cpu) == 8);
    assert(cpu.pc == 0x4567);

    cpu.pc = 0;
    bus.memory[0] = 0xfd;
    bus.memory[1] = 0xf9; /* LD SP,IY */
    cpu.iy = 0x789a;
    assert(z80_step(&cpu) == 10);
    assert(cpu.sp == 0x789a);
}

int main(void) {
    test_index_16bit();
    test_index_high_low();
    test_index_displacement();
    test_index_memory_h_l_rule();
    test_index_inc_alu();
    test_index_stack_control();

    puts("PASS z80 DD/FD IX/IY base instruction families");
    return 0;
}
