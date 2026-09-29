#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];

    uint16_t last_input_port;
    uint8_t input_value;
    uint32_t input_count;

    uint16_t last_output_port;
    uint8_t last_output_value;
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

    bus->last_input_port = port;
    bus->input_count++;

    return bus->input_value;
}

static void io_write(
    void *context,
    uint16_t port,
    uint8_t value
) {
    TestBus *bus = context;

    bus->last_output_port = port;
    bus->last_output_value = value;
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

static void test_in_register_c(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * IN D,(C)
     */
    load_ed(
        &bus,
        0x50
    );

    cpu.b = 0x12;
    cpu.c = 0x34;

    cpu.f = Z80_FLAG_C;

    bus.input_value = 0x80;

    assert(z80_step(&cpu) == 12);

    assert(bus.input_count == 1);
    assert(bus.last_input_port == 0x1234);

    assert(cpu.d == 0x80);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));
    assert(!(cpu.f & Z80_FLAG_H));
    assert(!(cpu.f & Z80_FLAG_N));
    assert(!(cpu.f & Z80_FLAG_PV));

    /*
     * Carry is preserved by IN r,(C).
     */
    assert(cpu.f & Z80_FLAG_C);

    /*
     * ED prefix and second opcode are both M1 fetches.
     */
    assert(cpu.r == 2);
}

static void test_in_c_discard(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * Undocumented IN (C).
     */
    load_ed(
        &bus,
        0x70
    );

    cpu.b = 0x40;
    cpu.c = 0x10;

    bus.input_value = 0x00;

    assert(z80_step(&cpu) == 12);

    assert(bus.input_count == 1);
    assert(bus.last_input_port == 0x4010);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_PV);
}

static void test_out_register_c(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * OUT (C),E
     */
    load_ed(
        &bus,
        0x59
    );

    cpu.b = 0x20;
    cpu.c = 0x30;
    cpu.e = 0xa5;

    assert(z80_step(&cpu) == 12);

    assert(bus.output_count == 1);
    assert(bus.last_output_port == 0x2030);
    assert(bus.last_output_value == 0xa5);

    /*
     * OUT (C),0
     */
    cpu.pc = 0;

    load_ed(
        &bus,
        0x71
    );

    bus.output_count = 0;

    assert(z80_step(&cpu) == 12);

    assert(bus.output_count == 1);
    assert(bus.last_output_port == 0x2030);
    assert(bus.last_output_value == 0x00);
}

static void test_adc_hl(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * ADC HL,BC
     */
    load_ed(
        &bus,
        0x4a
    );

    cpu.h = 0x7f;
    cpu.l = 0xff;

    cpu.b = 0x00;
    cpu.c = 0x00;

    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 15);

    assert(cpu.h == 0x80);
    assert(cpu.l == 0x00);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));

    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);

    assert(!(cpu.f & Z80_FLAG_N));
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_sbc_hl(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * SBC HL,DE
     */
    load_ed(
        &bus,
        0x52
    );

    cpu.h = 0x00;
    cpu.l = 0x00;

    cpu.d = 0x00;
    cpu.e = 0x00;

    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 15);

    assert(cpu.h == 0xff);
    assert(cpu.l == 0xff);

    assert(cpu.f & Z80_FLAG_S);
    assert(!(cpu.f & Z80_FLAG_Z));

    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_N);
    assert(cpu.f & Z80_FLAG_C);
}

static void test_store_register_pair(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * LD (2000h),DE
     *
     * ED 53 00 20
     */
    bus.memory[0] = 0xed;
    bus.memory[1] = 0x53;
    bus.memory[2] = 0x00;
    bus.memory[3] = 0x20;

    cpu.d = 0x12;
    cpu.e = 0x34;

    assert(z80_step(&cpu) == 20);

    assert(bus.memory[0x2000] == 0x34);
    assert(bus.memory[0x2001] == 0x12);
}

static void test_load_register_pair(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * LD SP,(3000h)
     *
     * ED 7B 00 30
     */
    bus.memory[0] = 0xed;
    bus.memory[1] = 0x7b;
    bus.memory[2] = 0x00;
    bus.memory[3] = 0x30;

    bus.memory[0x3000] = 0x78;
    bus.memory[0x3001] = 0x56;

    cpu.sp = 0;

    assert(z80_step(&cpu) == 20);

    assert(cpu.sp == 0x5678);
}

int main(void) {
    test_in_register_c();
    test_in_c_discard();

    test_out_register_c();

    test_adc_hl();
    test_sbc_hl();

    test_store_register_pair();
    test_load_register_pair();

    puts(
        "PASS z80 ED I/O, 16-bit arithmetic and memory transfers"
    );

    return 0;
}
