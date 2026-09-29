#ifndef ARCADE_CORE_Z80_H
#define ARCADE_CORE_Z80_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Z80 flag register F.
 */
enum {
    Z80_FLAG_C  = 0x01u,
    Z80_FLAG_N  = 0x02u,
    Z80_FLAG_PV = 0x04u,
    Z80_FLAG_X  = 0x08u,
    Z80_FLAG_H  = 0x10u,
    Z80_FLAG_Y  = 0x20u,
    Z80_FLAG_Z  = 0x40u,
    Z80_FLAG_S  = 0x80u
};

typedef uint8_t (*Z80MemoryReadFn)(
    void *context,
    uint16_t address
);

typedef void (*Z80MemoryWriteFn)(
    void *context,
    uint16_t address,
    uint8_t value
);

typedef uint8_t (*Z80IoReadFn)(
    void *context,
    uint16_t port
);

typedef void (*Z80IoWriteFn)(
    void *context,
    uint16_t port,
    uint8_t value
);

typedef uint8_t (*Z80InterruptAcknowledgeFn)(
    void *context
);

typedef void (*Z80RetiFn)(
    void *context
);

typedef struct {
    void *context;

    Z80MemoryReadFn memory_read;
    Z80MemoryWriteFn memory_write;

    Z80IoReadFn io_read;
    Z80IoWriteFn io_write;

    Z80InterruptAcknowledgeFn interrupt_acknowledge;
    Z80RetiFn reti;
} Z80Bus;

typedef struct {
    /* Primary register set. */
    uint8_t a;
    uint8_t f;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;

    /* Alternate register set. */
    uint8_t a_alt;
    uint8_t f_alt;
    uint8_t b_alt;
    uint8_t c_alt;
    uint8_t d_alt;
    uint8_t e_alt;
    uint8_t h_alt;
    uint8_t l_alt;

    /* Index, stack and program registers. */
    uint16_t ix;
    uint16_t iy;
    uint16_t sp;
    uint16_t pc;

    /* Interrupt and refresh registers. */
    uint8_t i;
    uint8_t r;

    /* Interrupt flip-flops and interrupt mode. */
    bool iff1;
    bool iff2;
    uint8_t interrupt_mode;

    /* CPU execution state. */
    bool halted;
    bool irq_line;
    bool nmi_pending;

    /*
     * EI does not enable maskable interrupts until after the following
     * instruction.
     */
    uint8_t ei_delay;

    /*
     * Internal WZ/MEMPTR state used by some instructions and
     * undocumented behavior.
     */
    uint16_t wz;

    /*
     * Internal Q state used by undocumented flag behavior.
     */
    uint8_t q;

    /*
     * Total Z80 T-states executed since reset.
     */
    uint64_t total_t_states;

    Z80Bus bus;
} Z80;

void z80_init(
    Z80 *cpu,
    const Z80Bus *bus
);

void z80_reset(
    Z80 *cpu
);

uint32_t z80_step(
    Z80 *cpu
);

void z80_set_irq(
    Z80 *cpu,
    bool asserted
);

void z80_pulse_nmi(
    Z80 *cpu
);

#endif
