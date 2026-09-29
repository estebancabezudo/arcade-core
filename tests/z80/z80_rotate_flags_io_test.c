#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];

    uint16_t input_port;
    uint8_t input_value;
    uint32_t input_count;
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

static Z80 create_cpu(
    TestBus *test_bus
) {
    Z80 cpu;

    const Z80Bus bus = {
        .context = test_bus,
        .memory_read = memory_read,
        .memory_write = memory_write,
        .io_read = io_read,
        .io_write = 0,
        .interrupt_acknowledge = 0,
        .reti = 0
    };

    z80_init(&cpu, &bus);

    return cpu;
}

static void test_rotates(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RLCA
     */
    bus.memory[0] = 0x07;

    cpu.a = 0x81;
    cpu.f = Z80_FLAG_Z;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x03);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_Z);

    /*
     * RRCA
     */
    cpu.pc = 0;
    bus.memory[0] = 0x0f;

    cpu.a = 0x01;
    cpu.f = Z80_FLAG_Z;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x80);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_Z);

    /*
     * RLA with old carry.
     */
    cpu.pc = 0;
    bus.memory[0] = 0x17;

    cpu.a = 0x80;
    cpu.f =
        Z80_FLAG_Z |
        Z80_FLAG_C;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x01);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_Z);

    /*
     * RRA with old carry.
     */
    cpu.pc = 0;
    bus.memory[0] = 0x1f;

    cpu.a = 0x01;
    cpu.f =
        Z80_FLAG_Z |
        Z80_FLAG_C;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x80);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_Z);
}

static void test_daa(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * 09 + 01 = 0A binary.
     * DAA converts it to BCD 10h.
     */
    cpu.a = 0x09;

    bus.memory[0] = 0xc6; /* ADD A,01 */
    bus.memory[1] = 0x01;
    bus.memory[2] = 0x27; /* DAA */

    assert(z80_step(&cpu) == 7);
    assert(cpu.a == 0x0a);

    assert(z80_step(&cpu) == 4);
    assert(cpu.a == 0x10);

    /*
     * 15 - 06 = 0F binary.
     * DAA after subtraction gives BCD 09.
     */
    z80_reset(&cpu);

    cpu.a = 0x15;

    bus.memory[0] = 0xd6; /* SUB 06 */
    bus.memory[1] = 0x06;
    bus.memory[2] = 0x27; /* DAA */

    assert(z80_step(&cpu) == 7);
    assert(cpu.a == 0x0f);

    assert(z80_step(&cpu) == 4);
    assert(cpu.a == 0x09);
    assert(cpu.f & Z80_FLAG_N);
}

static void test_flag_operations(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * CPL
     */
    bus.memory[0] = 0x2f;

    cpu.a = 0x55;
    cpu.f =
        Z80_FLAG_Z |
        Z80_FLAG_C;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0xaa);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_C);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_N);

    /*
     * SCF
     */
    cpu.pc = 0;
    bus.memory[0] = 0x37;

    cpu.f =
        Z80_FLAG_Z |
        Z80_FLAG_H |
        Z80_FLAG_N;

    assert(z80_step(&cpu) == 4);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_C);
    assert(!(cpu.f & Z80_FLAG_H));
    assert(!(cpu.f & Z80_FLAG_N));

    /*
     * CCF with carry set.
     */
    cpu.pc = 0;
    bus.memory[0] = 0x3f;

    cpu.f =
        Z80_FLAG_Z |
        Z80_FLAG_C;

    assert(z80_step(&cpu) == 4);

    assert(cpu.f & Z80_FLAG_Z);
    assert(!(cpu.f & Z80_FLAG_C));
    assert(cpu.f & Z80_FLAG_H);

    /*
     * CCF with carry clear.
     */
    cpu.pc = 0;
    bus.memory[0] = 0x3f;

    cpu.f = Z80_FLAG_Z;

    assert(z80_step(&cpu) == 4);

    assert(cpu.f & Z80_FLAG_Z);
    assert(cpu.f & Z80_FLAG_C);
    assert(!(cpu.f & Z80_FLAG_H));
}

static void test_immediate_adc_sbc(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * ADC A,00 with carry set:
     * 7F + 00 + 1 = 80
     */
    bus.memory[0] = 0xce;
    bus.memory[1] = 0x00;

    cpu.a = 0x7f;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 7);

    assert(cpu.a == 0x80);
    assert(cpu.f & Z80_FLAG_H);
    assert(cpu.f & Z80_FLAG_PV);

    /*
     * SBC A,00 with carry set:
     * 00 - 00 - 1 = FF
     */
    cpu.pc = 0;
    bus.memory[0] = 0xde;
    bus.memory[1] = 0x00;

    cpu.a = 0x00;
    cpu.f = Z80_FLAG_C;

    assert(z80_step(&cpu) == 7);

    assert(cpu.a == 0xff);
    assert(cpu.f & Z80_FLAG_N);
    assert(cpu.f & Z80_FLAG_C);
}

static void test_in_a_immediate(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * IN A,(10h)
     *
     * Existing A=42h is placed on the upper address bus,
     * therefore port is 4210h.
     */
    bus.memory[0] = 0xdb;
    bus.memory[1] = 0x10;

    cpu.a = 0x42;
    cpu.f = 0xa5;

    bus.input_value = 0x7e;

    assert(z80_step(&cpu) == 11);

    assert(bus.input_count == 1);
    assert(bus.input_port == 0x4210);

    assert(cpu.a == 0x7e);

    /*
     * IN A,(n) does not modify flags.
     */
    assert(cpu.f == 0xa5);
}

int main(void) {
    test_rotates();
    test_daa();

    test_flag_operations();

    test_immediate_adc_sbc();
    test_in_a_immediate();

    puts(
        "PASS z80 accumulator rotates, flags and immediate I/O"
    );

    return 0;
}
