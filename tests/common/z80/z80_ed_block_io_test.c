#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];

    uint16_t input_port;
    uint8_t input_value;
    uint32_t input_count;

    uint16_t output_port;
    uint8_t output_value;
    uint32_t output_count;
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

static uint8_t io_read(
    void *context,
    uint16_t port
) {
    TestBus *bus = context;

    bus->input_port = port;
    bus->input_count++;

    return bus->input_value;
}

static void io_write(
    void *context,
    uint16_t port,
    uint8_t value
) {
    TestBus *bus = context;

    bus->output_port = port;
    bus->output_value = value;
    bus->output_count++;
}

static Z80 create_cpu(
    TestBus *test_bus
) {
    Z80 cpu;

    const Z80Bus bus = {
        .context = test_bus,
        .memory_read = memory_read,
        .memory_write = memory_write,
        .io_read = io_read,
        .io_write = io_write,
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

static uint16_t hl(
    const Z80 *cpu
) {
    return (uint16_t)(
        ((uint16_t)cpu->h << 8) |
        cpu->l
    );
}

static void test_ini(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(&bus, 0xa2);

    cpu.b = 0x02;
    cpu.c = 0x10;

    cpu.h = 0x40;
    cpu.l = 0x00;

    bus.input_value = 0x55;

    assert(z80_step(&cpu) == 16);

    /*
     * INI uses BC after decrementing B.
     */
    assert(bus.input_port == 0x0110);

    assert(cpu.b == 0x01);
    assert(hl(&cpu) == 0x4001);

    assert(bus.memory[0x4000] == 0x55);
}

static void test_ind(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(&bus, 0xaa);

    cpu.b = 0x01;
    cpu.c = 0x20;

    cpu.h = 0x40;
    cpu.l = 0x02;

    bus.input_value = 0x80;

    assert(z80_step(&cpu) == 16);

    assert(bus.input_port == 0x0020);

    assert(cpu.b == 0x00);
    assert(hl(&cpu) == 0x4001);

    assert(bus.memory[0x4002] == 0x80);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_N);
}

static void test_inir(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(&bus, 0xb2);

    cpu.b = 0x02;
    cpu.c = 0x30;

    cpu.h = 0x40;
    cpu.l = 0x00;

    bus.input_value = 0x11;

    assert(z80_step(&cpu) == 21);

    assert(cpu.pc == 0);
    assert(cpu.b == 1);
    assert(hl(&cpu) == 0x4001);

    bus.input_value = 0x22;

    assert(z80_step(&cpu) == 16);

    assert(cpu.pc == 2);
    assert(cpu.b == 0);

    assert(bus.memory[0x4000] == 0x11);
    assert(bus.memory[0x4001] == 0x22);
}

static void test_indr(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(&bus, 0xba);

    cpu.b = 0x02;
    cpu.c = 0x40;

    cpu.h = 0x40;
    cpu.l = 0x01;

    bus.input_value = 0xaa;

    assert(z80_step(&cpu) == 21);

    bus.input_value = 0xbb;

    assert(z80_step(&cpu) == 16);

    assert(cpu.b == 0);
    assert(hl(&cpu) == 0x3fff);

    assert(bus.memory[0x4001] == 0xaa);
    assert(bus.memory[0x4000] == 0xbb);
}

static void test_outi(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(&bus, 0xa3);

    cpu.b = 0x02;
    cpu.c = 0x10;

    cpu.h = 0x40;
    cpu.l = 0x01;

    bus.memory[0x4001] = 0xff;

    assert(z80_step(&cpu) == 16);

    /*
     * OUTI presents the original BC.
     */
    assert(bus.output_port == 0x0210);
    assert(bus.output_value == 0xff);

    assert(cpu.b == 1);
    assert(hl(&cpu) == 0x4002);

    /*
     * FF + new L(02) overflows.
     */
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_N);
}

static void test_outd(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(&bus, 0xab);

    cpu.b = 0x01;
    cpu.c = 0x20;

    cpu.h = 0x40;
    cpu.l = 0x02;

    bus.memory[0x4002] = 0x01;

    assert(z80_step(&cpu) == 16);

    assert(bus.output_port == 0x0120);
    assert(bus.output_value == 0x01);

    assert(cpu.b == 0);
    assert(hl(&cpu) == 0x4001);

    assert(cpu.f & Z80_FLAG_Z);
    assert(!(cpu.f & Z80_FLAG_N));
}

static void test_otir(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(&bus, 0xb3);

    cpu.b = 0x02;
    cpu.c = 0x30;

    cpu.h = 0x40;
    cpu.l = 0x00;

    bus.memory[0x4000] = 0x11;
    bus.memory[0x4001] = 0x22;

    assert(z80_step(&cpu) == 21);

    assert(cpu.pc == 0);
    assert(bus.output_port == 0x0230);
    assert(bus.output_value == 0x11);

    assert(z80_step(&cpu) == 16);

    assert(cpu.pc == 2);
    assert(bus.output_port == 0x0130);
    assert(bus.output_value == 0x22);

    assert(cpu.b == 0);
    assert(hl(&cpu) == 0x4002);
}

static void test_otdr(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    load_ed(&bus, 0xbb);

    cpu.b = 0x02;
    cpu.c = 0x40;

    cpu.h = 0x40;
    cpu.l = 0x01;

    bus.memory[0x4001] = 0xaa;
    bus.memory[0x4000] = 0xbb;

    assert(z80_step(&cpu) == 21);
    assert(z80_step(&cpu) == 16);

    assert(cpu.b == 0);
    assert(hl(&cpu) == 0x3fff);

    assert(bus.output_value == 0xbb);
}

int main(void) {
    test_ini();
    test_ind();

    test_inir();
    test_indr();

    test_outi();
    test_outd();

    test_otir();
    test_otdr();

    puts("PASS z80 ED block I/O");

    return 0;
}
