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

static void test_jp(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * JP 1234h
     */
    bus.memory[0x0000] = 0xc3;
    bus.memory[0x0001] = 0x34;
    bus.memory[0x0002] = 0x12;

    assert(z80_step(&cpu) == 10);
    assert(cpu.pc == 0x1234);
    assert(cpu.r == 1);
}

static void test_jr_forward(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * JR +5
     *
     * PC after reading displacement is 0002h,
     * therefore destination is 0007h.
     */
    bus.memory[0x0000] = 0x18;
    bus.memory[0x0001] = 0x05;

    assert(z80_step(&cpu) == 12);
    assert(cpu.pc == 0x0007);
}

static void test_jr_backward(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * JR -2
     *
     * PC after reading displacement is 0002h.
     * 0002h - 2 = 0000h.
     */
    bus.memory[0x0000] = 0x18;
    bus.memory[0x0001] = 0xfe;

    assert(z80_step(&cpu) == 12);
    assert(cpu.pc == 0x0000);
}

static void test_call(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * CALL 5678h
     */
    bus.memory[0x0000] = 0xcd;
    bus.memory[0x0001] = 0x78;
    bus.memory[0x0002] = 0x56;

    cpu.sp = 0x9000;

    assert(z80_step(&cpu) == 17);

    assert(cpu.pc == 0x5678);
    assert(cpu.sp == 0x8ffe);

    /*
     * Return address is 0003h.
     *
     * Z80 pushes high byte first, then low byte.
     */
    assert(bus.memory[0x8fff] == 0x00);
    assert(bus.memory[0x8ffe] == 0x03);
}

static void test_ret(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * RET
     */
    bus.memory[0x0000] = 0xc9;

    cpu.sp = 0x8000;

    /*
     * Stack contains return address 1234h.
     */
    bus.memory[0x8000] = 0x34;
    bus.memory[0x8001] = 0x12;

    assert(z80_step(&cpu) == 10);

    assert(cpu.pc == 0x1234);
    assert(cpu.sp == 0x8002);
}

static void test_call_then_ret(void) {
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    /*
     * 0000: CALL 0100h
     * 0003: HALT
     *
     * 0100: RET
     */
    bus.memory[0x0000] = 0xcd;
    bus.memory[0x0001] = 0x00;
    bus.memory[0x0002] = 0x01;
    bus.memory[0x0003] = 0x76;

    bus.memory[0x0100] = 0xc9;

    cpu.sp = 0x9000;

    assert(z80_step(&cpu) == 17);
    assert(cpu.pc == 0x0100);

    assert(z80_step(&cpu) == 10);
    assert(cpu.pc == 0x0003);

    assert(z80_step(&cpu) == 4);
    assert(cpu.halted);

    assert(cpu.sp == 0x9000);
    assert(cpu.total_t_states == 31);
}

int main(void) {
    test_jp();

    test_jr_forward();
    test_jr_backward();

    test_call();
    test_ret();
    test_call_then_ret();

    puts("PASS z80 flow control");

    return 0;
}
