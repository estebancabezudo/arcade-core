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
        .memory_write = memory_write
    };

    z80_init(&cpu, &bus);

    return cpu;
}

static void test_subtract_low_invalid_bcd_state(void)
{
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0x27; /* DAA */

    cpu.a = 0x0a;
    cpu.f = Z80_FLAG_N;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x04);
    assert(cpu.f & Z80_FLAG_N);
    assert(!(cpu.f & Z80_FLAG_C));
}

static void test_subtract_high_invalid_bcd_state(void)
{
    TestBus bus = {0};
    Z80 cpu = create_cpu(&bus);

    bus.memory[0] = 0x27; /* DAA */

    cpu.a = 0xa0;
    cpu.f = Z80_FLAG_N;

    assert(z80_step(&cpu) == 4);

    assert(cpu.a == 0x40);
    assert(cpu.f & Z80_FLAG_N);
    assert(cpu.f & Z80_FLAG_C);
}

int main(void)
{
    test_subtract_low_invalid_bcd_state();
    test_subtract_high_invalid_bcd_state();

    puts("PASS z80 DAA edge states");

    return 0;
}
