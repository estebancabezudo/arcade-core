#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "z80.h"

typedef struct {
    uint8_t memory[65536];
    uint8_t interrupt_vector;
} TestBus;

static uint8_t test_memory_read(
    void *context,
    uint16_t address
) {
    TestBus *bus = context;
    return bus->memory[address];
}

static void test_memory_write(
    void *context,
    uint16_t address,
    uint8_t value
) {
    TestBus *bus = context;
    bus->memory[address] = value;
}

static uint8_t test_interrupt_acknowledge(
    void *context
) {
    TestBus *bus = context;
    return bus->interrupt_vector;
}

static Z80 create_cpu(
    TestBus *test_bus
) {
    Z80 cpu;

    Z80Bus bus = {
        .context = test_bus,
        .memory_read = test_memory_read,
        .memory_write = test_memory_write,
        .io_read = 0,
        .io_write = 0,
        .interrupt_acknowledge =
            test_interrupt_acknowledge,
        .reti = 0
    };

    z80_init(
        &cpu,
        &bus
    );

    return cpu;
}

static void test_reset(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.a = 0x12;
    cpu.pc = 0x3456;
    cpu.sp = 0x789a;
    cpu.iff1 = true;
    cpu.halted = true;
    cpu.total_t_states = 1234;

    z80_reset(&cpu);

    assert(cpu.a == 0);
    assert(cpu.pc == 0);
    assert(cpu.sp == 0);
    assert(cpu.iff1 == false);
    assert(cpu.iff2 == false);
    assert(cpu.halted == false);
    assert(cpu.interrupt_mode == 0);
    assert(cpu.total_t_states == 0);

    /*
     * Reset must preserve the external bus connection.
     */
    bus.memory[0x1234] = 0x5a;

    assert(
        cpu.bus.memory_read(
            cpu.bus.context,
            0x1234
        ) == 0x5a
    );
}

static void test_nop(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0x0000] = 0x00;

    const uint32_t cycles =
        z80_step(&cpu);

    assert(cycles == 4);
    assert(cpu.pc == 0x0001);
    assert(cpu.r == 0x01);
    assert(cpu.total_t_states == 4);
}

static void test_halt(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0x0000] = 0x76;

    assert(z80_step(&cpu) == 4);

    assert(cpu.halted);
    assert(cpu.pc == 0x0001);
    assert(cpu.r == 0x01);

    assert(z80_step(&cpu) == 4);

    assert(cpu.halted);
    assert(cpu.pc == 0x0001);
    assert(cpu.r == 0x02);
    assert(cpu.total_t_states == 8);
}

static void test_nmi(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.pc = 0x1234;
    cpu.sp = 0x8000;
    cpu.iff1 = true;
    cpu.iff2 = false;

    z80_pulse_nmi(&cpu);

    const uint32_t cycles =
        z80_step(&cpu);

    assert(cycles == 11);

    assert(cpu.pc == 0x0066);
    assert(cpu.sp == 0x7ffe);

    assert(bus.memory[0x7fff] == 0x12);
    assert(bus.memory[0x7ffe] == 0x34);

    assert(cpu.iff1 == false);

    /*
     * NMI preserves IFF2; it does not overwrite it
     * with the previous value of IFF1.
     */
    assert(cpu.iff2 == false);

    assert(cpu.nmi_pending == false);

    assert(cpu.r == 0x01);
    assert(cpu.total_t_states == 11);
}

static void test_interrupt_mode_1(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.pc = 0x2468;
    cpu.sp = 0x9000;

    cpu.interrupt_mode = 1;
    cpu.iff1 = true;
    cpu.iff2 = true;

    z80_set_irq(
        &cpu,
        true
    );

    const uint32_t cycles =
        z80_step(&cpu);

    assert(cycles == 13);

    assert(cpu.pc == 0x0038);
    assert(cpu.sp == 0x8ffe);

    assert(bus.memory[0x8fff] == 0x24);
    assert(bus.memory[0x8ffe] == 0x68);

    assert(cpu.iff1 == false);
    assert(cpu.iff2 == false);

    assert(cpu.r == 0x01);
    assert(cpu.total_t_states == 13);
}

static void test_interrupt_mode_2(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    cpu.pc = 0x1357;
    cpu.sp = 0xa000;

    cpu.i = 0x40;
    cpu.interrupt_mode = 2;

    cpu.iff1 = true;
    cpu.iff2 = true;

    bus.interrupt_vector = 0x20;

    bus.memory[0x4020] = 0xcd;
    bus.memory[0x4021] = 0xab;

    z80_set_irq(
        &cpu,
        true
    );

    const uint32_t cycles =
        z80_step(&cpu);

    assert(cycles == 19);

    assert(cpu.pc == 0xabcd);
    assert(cpu.sp == 0x9ffe);

    assert(bus.memory[0x9fff] == 0x13);
    assert(bus.memory[0x9ffe] == 0x57);

    assert(cpu.iff1 == false);
    assert(cpu.iff2 == false);

    assert(cpu.r == 0x01);
    assert(cpu.total_t_states == 19);
}

int main(void) {
    test_reset();
    test_nop();
    test_halt();
    test_nmi();
    test_interrupt_mode_1();
    test_interrupt_mode_2();

    puts("PASS z80 core bootstrap");

    return 0;
}
