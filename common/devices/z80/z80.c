#include "z80.h"

static uint8_t memory_read(
    Z80 *cpu,
    uint16_t address
) {
    if (cpu->bus.memory_read == 0) {
        return 0xff;
    }

    return cpu->bus.memory_read(
        cpu->bus.context,
        address
    );
}

static void memory_write(
    Z80 *cpu,
    uint16_t address,
    uint8_t value
) {
    if (cpu->bus.memory_write == 0) {
        return;
    }

    cpu->bus.memory_write(
        cpu->bus.context,
        address,
        value
    );
}

static uint8_t io_read(
    Z80 *cpu,
    uint16_t port
) {
    if (cpu->bus.io_read == 0) {
        return 0xff;
    }

    return cpu->bus.io_read(
        cpu->bus.context,
        port
    );
}

static void io_write(
    Z80 *cpu,
    uint16_t port,
    uint8_t value
) {
    if (cpu->bus.io_write == 0) {
        return;
    }

    cpu->bus.io_write(
        cpu->bus.context,
        port,
        value
    );
}

static uint8_t interrupt_acknowledge(
    Z80 *cpu
) {
    if (cpu->bus.interrupt_acknowledge == 0) {
        return 0xff;
    }

    return cpu->bus.interrupt_acknowledge(
        cpu->bus.context
    );
}

static uint16_t get_hl(
    const Z80 *cpu
) {
    return (uint16_t)(
        ((uint16_t)cpu->h << 8) |
        cpu->l
    );
}

static uint16_t get_register_pair(
    const Z80 *cpu,
    uint8_t pair
) {
    switch (pair & 3u) {
        case 0u:
            return (uint16_t)(
                ((uint16_t)cpu->b << 8) |
                cpu->c
            );

        case 1u:
            return (uint16_t)(
                ((uint16_t)cpu->d << 8) |
                cpu->e
            );

        case 2u:
            return get_hl(cpu);

        case 3u:
            return cpu->sp;

        default:
            return 0u;
    }
}

static void set_register_pair(
    Z80 *cpu,
    uint8_t pair,
    uint16_t value
) {
    const uint8_t high =
        (uint8_t)(value >> 8);

    const uint8_t low =
        (uint8_t)value;

    switch (pair & 3u) {
        case 0u:
            cpu->b = high;
            cpu->c = low;
            break;

        case 1u:
            cpu->d = high;
            cpu->e = low;
            break;

        case 2u:
            cpu->h = high;
            cpu->l = low;
            break;

        case 3u:
            cpu->sp = value;
            break;

        default:
            break;
    }
}

static uint16_t get_stack_pair(
    const Z80 *cpu,
    uint8_t pair
) {
    switch (pair & 3u) {
        case 0u:
            return (uint16_t)(
                ((uint16_t)cpu->b << 8) |
                cpu->c
            );

        case 1u:
            return (uint16_t)(
                ((uint16_t)cpu->d << 8) |
                cpu->e
            );

        case 2u:
            return (uint16_t)(
                ((uint16_t)cpu->h << 8) |
                cpu->l
            );

        case 3u:
            return (uint16_t)(
                ((uint16_t)cpu->a << 8) |
                cpu->f
            );

        default:
            return 0u;
    }
}

static void set_stack_pair(
    Z80 *cpu,
    uint8_t pair,
    uint16_t value
) {
    const uint8_t high =
        (uint8_t)(value >> 8);

    const uint8_t low =
        (uint8_t)value;

    switch (pair & 3u) {
        case 0u:
            cpu->b = high;
            cpu->c = low;
            break;

        case 1u:
            cpu->d = high;
            cpu->e = low;
            break;

        case 2u:
            cpu->h = high;
            cpu->l = low;
            break;

        case 3u:
            cpu->a = high;
            cpu->f = low;
            break;

        default:
            break;
    }
}

static void increment_r(
    Z80 *cpu
) {
    cpu->r =
        (uint8_t)(
            (cpu->r & 0x80u) |
            ((cpu->r + 1u) & 0x7fu)
        );
}

static void decrement_r(
    Z80 *cpu
) {
    cpu->r =
        (uint8_t)(
            (cpu->r & 0x80u) |
            ((cpu->r - 1u) & 0x7fu)
        );
}

static uint8_t fetch_byte(
    Z80 *cpu
) {
    const uint8_t value =
        memory_read(
            cpu,
            cpu->pc
        );

    cpu->pc++;

    return value;
}

static uint16_t fetch_word(
    Z80 *cpu
) {
    const uint8_t low =
        fetch_byte(cpu);

    const uint8_t high =
        fetch_byte(cpu);

    return (uint16_t)(
        ((uint16_t)high << 8) |
        low
    );
}

static void push_word(
    Z80 *cpu,
    uint16_t value
) {
    cpu->sp--;

    memory_write(
        cpu,
        cpu->sp,
        (uint8_t)(value >> 8)
    );

    cpu->sp--;

    memory_write(
        cpu,
        cpu->sp,
        (uint8_t)value
    );
}

static uint16_t pop_word(
    Z80 *cpu
) {
    const uint8_t low =
        memory_read(
            cpu,
            cpu->sp
        );

    cpu->sp++;

    const uint8_t high =
        memory_read(
            cpu,
            cpu->sp
        );

    cpu->sp++;

    return (uint16_t)(
        ((uint16_t)high << 8) |
        low
    );
}

static uint8_t read_register_code(
    Z80 *cpu,
    uint8_t code
) {
    switch (code & 7u) {
        case 0u:
            return cpu->b;

        case 1u:
            return cpu->c;

        case 2u:
            return cpu->d;

        case 3u:
            return cpu->e;

        case 4u:
            return cpu->h;

        case 5u:
            return cpu->l;

        case 6u:
            return memory_read(
                cpu,
                get_hl(cpu)
            );

        case 7u:
            return cpu->a;

        default:
            return 0xff;
    }
}

static void write_register_code(
    Z80 *cpu,
    uint8_t code,
    uint8_t value
) {
    switch (code & 7u) {
        case 0u:
            cpu->b = value;
            break;

        case 1u:
            cpu->c = value;
            break;

        case 2u:
            cpu->d = value;
            break;

        case 3u:
            cpu->e = value;
            break;

        case 4u:
            cpu->h = value;
            break;

        case 5u:
            cpu->l = value;
            break;

        case 6u:
            memory_write(
                cpu,
                get_hl(cpu),
                value
            );
            break;

        case 7u:
            cpu->a = value;
            break;

        default:
            break;
    }
}

static bool parity_even(
    uint8_t value
) {
    bool parity = true;

    for (uint8_t bit = 0; bit < 8u; bit++) {
        if (
            value &
            (uint8_t)(1u << bit)
        ) {
            parity = !parity;
        }
    }

    return parity;
}

static uint8_t result_szxy_flags(
    uint8_t result
) {
    uint8_t flags =
        result &
        (Z80_FLAG_S |
         Z80_FLAG_Y |
         Z80_FLAG_X);

    if (result == 0u) {
        flags |= Z80_FLAG_Z;
    }

    return flags;
}

static void rotate_accumulator_left_circular(
    Z80 *cpu
) {
    const uint8_t old_a =
        cpu->a;

    const uint8_t carry =
        (uint8_t)(old_a >> 7);

    cpu->a =
        (uint8_t)(
            (old_a << 1) |
            carry
        );

    cpu->f =
        (uint8_t)(
            (cpu->f &
             (
                 Z80_FLAG_S |
                 Z80_FLAG_Z |
                 Z80_FLAG_PV
             )) |
            (cpu->a &
             (
                 Z80_FLAG_X |
                 Z80_FLAG_Y
             )) |
            (carry ? Z80_FLAG_C : 0u)
        );

    cpu->q = cpu->f;
}

static void rotate_accumulator_right_circular(
    Z80 *cpu
) {
    const uint8_t old_a =
        cpu->a;

    const uint8_t carry =
        old_a & 1u;

    cpu->a =
        (uint8_t)(
            (old_a >> 1) |
            (carry << 7)
        );

    cpu->f =
        (uint8_t)(
            (cpu->f &
             (
                 Z80_FLAG_S |
                 Z80_FLAG_Z |
                 Z80_FLAG_PV
             )) |
            (cpu->a &
             (
                 Z80_FLAG_X |
                 Z80_FLAG_Y
             )) |
            (carry ? Z80_FLAG_C : 0u)
        );

    cpu->q = cpu->f;
}

static void rotate_accumulator_left(
    Z80 *cpu
) {
    const uint8_t old_a =
        cpu->a;

    const uint8_t old_carry =
        (cpu->f & Z80_FLAG_C)
            ? 1u
            : 0u;

    const uint8_t new_carry =
        (uint8_t)(old_a >> 7);

    cpu->a =
        (uint8_t)(
            (old_a << 1) |
            old_carry
        );

    cpu->f =
        (uint8_t)(
            (cpu->f &
             (
                 Z80_FLAG_S |
                 Z80_FLAG_Z |
                 Z80_FLAG_PV
             )) |
            (cpu->a &
             (
                 Z80_FLAG_X |
                 Z80_FLAG_Y
             )) |
            (new_carry ? Z80_FLAG_C : 0u)
        );

    cpu->q = cpu->f;
}

static void rotate_accumulator_right(
    Z80 *cpu
) {
    const uint8_t old_a =
        cpu->a;

    const uint8_t old_carry =
        (cpu->f & Z80_FLAG_C)
            ? 1u
            : 0u;

    const uint8_t new_carry =
        old_a & 1u;

    cpu->a =
        (uint8_t)(
            (old_a >> 1) |
            (old_carry << 7)
        );

    cpu->f =
        (uint8_t)(
            (cpu->f &
             (
                 Z80_FLAG_S |
                 Z80_FLAG_Z |
                 Z80_FLAG_PV
             )) |
            (cpu->a &
             (
                 Z80_FLAG_X |
                 Z80_FLAG_Y
             )) |
            (new_carry ? Z80_FLAG_C : 0u)
        );

    cpu->q = cpu->f;
}

static void decimal_adjust_accumulator(
    Z80 *cpu
) {
    const uint8_t old_a =
        cpu->a;

    const bool subtract =
        (cpu->f & Z80_FLAG_N) != 0u;

    const bool old_half =
        (cpu->f & Z80_FLAG_H) != 0u;

    bool carry =
        (cpu->f & Z80_FLAG_C) != 0u;

    uint8_t correction = 0u;

    /*
     * Real Z80 DAA behavior is defined for every possible
     * A/H/N/C state, not only states normally produced by
     * valid BCD arithmetic.
     */
    if (
        old_half ||
        (old_a & 0x0fu) > 9u
    ) {
        correction |= 0x06u;
    }

    if (
        carry ||
        old_a > 0x99u
    ) {
        correction |= 0x60u;
        carry = true;
    }

    uint8_t result;
    bool half;

    if (subtract) {
        /*
         * After subtraction H remains set only for the
         * corresponding low-nibble correction condition.
         */
        half =
            old_half &&
            (old_a & 0x0fu) < 0x06u;

        result =
            (uint8_t)(
                old_a -
                correction
            );
    } else {
        half =
            (old_a & 0x0fu) > 9u;

        result =
            (uint8_t)(
                old_a +
                correction
            );
    }

    uint8_t flags =
        result_szxy_flags(result);

    if (parity_even(result)) {
        flags |= Z80_FLAG_PV;
    }

    if (half) {
        flags |= Z80_FLAG_H;
    }

    if (subtract) {
        flags |= Z80_FLAG_N;
    }

    if (carry) {
        flags |= Z80_FLAG_C;
    }

    cpu->a = result;
    cpu->f = flags;
    cpu->q = flags;
}

static bool condition_true(
    const Z80 *cpu,
    uint8_t condition
) {
    switch (condition & 7u) {
        case 0u: /* NZ */
            return (cpu->f & Z80_FLAG_Z) == 0u;

        case 1u: /* Z */
            return (cpu->f & Z80_FLAG_Z) != 0u;

        case 2u: /* NC */
            return (cpu->f & Z80_FLAG_C) == 0u;

        case 3u: /* C */
            return (cpu->f & Z80_FLAG_C) != 0u;

        case 4u: /* PO */
            return (cpu->f & Z80_FLAG_PV) == 0u;

        case 5u: /* PE */
            return (cpu->f & Z80_FLAG_PV) != 0u;

        case 6u: /* P */
            return (cpu->f & Z80_FLAG_S) == 0u;

        case 7u: /* M */
            return (cpu->f & Z80_FLAG_S) != 0u;

        default:
            return false;
    }
}

static void alu_add_hl(
    Z80 *cpu,
    uint16_t value
) {
    const uint16_t hl =
        get_hl(cpu);

    const uint32_t wide_result =
        (uint32_t)hl +
        (uint32_t)value;

    const uint16_t result =
        (uint16_t)wide_result;

    /*
     * S, Z and P/V are unaffected.
     * N is cleared.
     */
    uint8_t flags =
        cpu->f &
        (
            Z80_FLAG_S |
            Z80_FLAG_Z |
            Z80_FLAG_PV
        );

    /*
     * Undocumented X/Y flags come from the high byte
     * of the 16-bit result.
     */
    flags |=
        (uint8_t)(
            (result >> 8) &
            (
                Z80_FLAG_X |
                Z80_FLAG_Y
            )
        );

    if (
        (
            (hl & 0x0fffu) +
            (value & 0x0fffu)
        ) >
        0x0fffu
    ) {
        flags |= Z80_FLAG_H;
    }

    if (wide_result > 0xffffu) {
        flags |= Z80_FLAG_C;
    }

    cpu->h =
        (uint8_t)(result >> 8);

    cpu->l =
        (uint8_t)result;

    cpu->f = flags;
    cpu->q = flags;

    cpu->wz =
        (uint16_t)(hl + 1u);
}

static uint8_t alu_inc(
    Z80 *cpu,
    uint8_t value
) {
    const uint8_t result =
        (uint8_t)(value + 1u);

    uint8_t flags =
        (cpu->f & Z80_FLAG_C) |
        result_szxy_flags(result);

    if ((value & 0x0fu) == 0x0fu) {
        flags |= Z80_FLAG_H;
    }

    if (value == 0x7fu) {
        flags |= Z80_FLAG_PV;
    }

    cpu->f = flags;
    cpu->q = flags;

    return result;
}

static uint8_t alu_dec(
    Z80 *cpu,
    uint8_t value
) {
    const uint8_t result =
        (uint8_t)(value - 1u);

    uint8_t flags =
        (cpu->f & Z80_FLAG_C) |
        result_szxy_flags(result) |
        Z80_FLAG_N;

    if ((value & 0x0fu) == 0x00u) {
        flags |= Z80_FLAG_H;
    }

    if (value == 0x80u) {
        flags |= Z80_FLAG_PV;
    }

    cpu->f = flags;
    cpu->q = flags;

    return result;
}

static void alu_add(
    Z80 *cpu,
    uint8_t value,
    uint8_t carry
) {
    const uint8_t a =
        cpu->a;

    const uint16_t wide_result =
        (uint16_t)a +
        (uint16_t)value +
        (uint16_t)carry;

    const uint8_t result =
        (uint8_t)wide_result;

    uint8_t flags =
        result_szxy_flags(result);

    if (
        ((a & 0x0fu) +
         (value & 0x0fu) +
         carry) >
        0x0fu
    ) {
        flags |= Z80_FLAG_H;
    }

    if (
        ((~(a ^ value)) &
         (a ^ result) &
         0x80u) != 0u
    ) {
        flags |= Z80_FLAG_PV;
    }

    if (wide_result > 0xffu) {
        flags |= Z80_FLAG_C;
    }

    cpu->a = result;
    cpu->f = flags;
    cpu->q = flags;
}

static void alu_sub(
    Z80 *cpu,
    uint8_t value,
    uint8_t borrow
) {
    const uint8_t a =
        cpu->a;

    const uint16_t subtrahend =
        (uint16_t)value +
        (uint16_t)borrow;

    const uint8_t result =
        (uint8_t)(
            (uint16_t)a -
            subtrahend
        );

    uint8_t flags =
        result_szxy_flags(result) |
        Z80_FLAG_N;

    if (
        ((a ^ value ^ result) &
         0x10u) != 0u
    ) {
        flags |= Z80_FLAG_H;
    }

    if (
        ((a ^ value) &
         (a ^ result) &
         0x80u) != 0u
    ) {
        flags |= Z80_FLAG_PV;
    }

    if (
        (uint16_t)a <
        subtrahend
    ) {
        flags |= Z80_FLAG_C;
    }

    cpu->a = result;
    cpu->f = flags;
    cpu->q = flags;
}

static void alu_and(
    Z80 *cpu,
    uint8_t value
) {
    const uint8_t result =
        (uint8_t)(
            cpu->a & value
        );

    uint8_t flags =
        result_szxy_flags(result) |
        Z80_FLAG_H;

    if (parity_even(result)) {
        flags |= Z80_FLAG_PV;
    }

    cpu->a = result;
    cpu->f = flags;
    cpu->q = flags;
}

static void alu_xor(
    Z80 *cpu,
    uint8_t value
) {
    const uint8_t result =
        (uint8_t)(
            cpu->a ^ value
        );

    uint8_t flags =
        result_szxy_flags(result);

    if (parity_even(result)) {
        flags |= Z80_FLAG_PV;
    }

    cpu->a = result;
    cpu->f = flags;
    cpu->q = flags;
}

static void alu_or(
    Z80 *cpu,
    uint8_t value
) {
    const uint8_t result =
        (uint8_t)(
            cpu->a | value
        );

    uint8_t flags =
        result_szxy_flags(result);

    if (parity_even(result)) {
        flags |= Z80_FLAG_PV;
    }

    cpu->a = result;
    cpu->f = flags;
    cpu->q = flags;
}

static void alu_cp(
    Z80 *cpu,
    uint8_t value
) {
    const uint8_t a =
        cpu->a;

    const uint8_t result =
        (uint8_t)(
            a - value
        );

    uint8_t flags =
        (result &
         Z80_FLAG_S) |
        (value &
         (Z80_FLAG_Y |
          Z80_FLAG_X)) |
        Z80_FLAG_N;

    if (result == 0u) {
        flags |= Z80_FLAG_Z;
    }

    if (
        ((a ^ value ^ result) &
         0x10u) != 0u
    ) {
        flags |= Z80_FLAG_H;
    }

    if (
        ((a ^ value) &
         (a ^ result) &
         0x80u) != 0u
    ) {
        flags |= Z80_FLAG_PV;
    }

    if (a < value) {
        flags |= Z80_FLAG_C;
    }

    cpu->f = flags;
    cpu->q = flags;
}

static uint32_t finish_step(
    Z80 *cpu,
    uint32_t t_states,
    bool completed_instruction
) {
    cpu->total_t_states += t_states;

    if (
        completed_instruction &&
        cpu->ei_delay > 0
    ) {
        cpu->ei_delay--;
    }

    return t_states;
}

static uint8_t cb_rotate_shift(
    Z80 *cpu,
    uint8_t operation,
    uint8_t value
) {
    const uint8_t old_carry =
        (cpu->f & Z80_FLAG_C)
            ? 1u
            : 0u;

    uint8_t result = value;
    uint8_t carry = 0u;

    switch (operation & 7u) {
        case 0u: /* RLC */
            carry =
                (uint8_t)(value >> 7);

            result =
                (uint8_t)(
                    (value << 1) |
                    carry
                );
            break;

        case 1u: /* RRC */
            carry =
                value & 1u;

            result =
                (uint8_t)(
                    (value >> 1) |
                    (carry << 7)
                );
            break;

        case 2u: /* RL */
            carry =
                (uint8_t)(value >> 7);

            result =
                (uint8_t)(
                    (value << 1) |
                    old_carry
                );
            break;

        case 3u: /* RR */
            carry =
                value & 1u;

            result =
                (uint8_t)(
                    (value >> 1) |
                    (old_carry << 7)
                );
            break;

        case 4u: /* SLA */
            carry =
                (uint8_t)(value >> 7);

            result =
                (uint8_t)(value << 1);
            break;

        case 5u: /* SRA */
            carry =
                value & 1u;

            result =
                (uint8_t)(
                    (value >> 1) |
                    (value & 0x80u)
                );
            break;

        case 6u: /* SLL / undocumented */
            carry =
                (uint8_t)(value >> 7);

            result =
                (uint8_t)(
                    (value << 1) |
                    1u
                );
            break;

        case 7u: /* SRL */
            carry =
                value & 1u;

            result =
                (uint8_t)(value >> 1);
            break;

        default:
            break;
    }

    uint8_t flags =
        result_szxy_flags(result);

    if (parity_even(result)) {
        flags |= Z80_FLAG_PV;
    }

    if (carry != 0u) {
        flags |= Z80_FLAG_C;
    }

    cpu->f = flags;
    cpu->q = flags;

    return result;
}

static uint32_t execute_cb(
    Z80 *cpu
) {
    /*
     * The byte following CB is another M1 opcode fetch,
     * so register R increments again.
     */
    const uint8_t opcode =
        memory_read(
            cpu,
            cpu->pc
        );

    cpu->pc++;
    increment_r(cpu);

    const uint8_t group =
        (uint8_t)(opcode >> 6);

    const uint8_t operation =
        (uint8_t)(
            (opcode >> 3) &
            7u
        );

    const uint8_t code =
        opcode & 7u;

    const bool memory_operand =
        code == 6u;

    const uint8_t value =
        read_register_code(
            cpu,
            code
        );

    /*
     * 00xxxxxx:
     * RLC/RRC/RL/RR/SLA/SRA/SLL/SRL
     */
    if (group == 0u) {
        const uint8_t result =
            cb_rotate_shift(
                cpu,
                operation,
                value
            );

        write_register_code(
            cpu,
            code,
            result
        );

        return finish_step(
            cpu,
            memory_operand
                ? 15u
                : 8u,
            true
        );
    }

    /*
     * 01xxxxxx:
     * BIT b,r
     */
    if (group == 1u) {
        const uint8_t mask =
            (uint8_t)(
                1u << operation
            );

        const bool bit_set =
            (value & mask) != 0u;

        uint8_t flags =
            (cpu->f & Z80_FLAG_C) |
            Z80_FLAG_H;

        if (!bit_set) {
            flags |=
                Z80_FLAG_Z |
                Z80_FLAG_PV;
        }

        if (
            operation == 7u &&
            bit_set
        ) {
            flags |= Z80_FLAG_S;
        }

        /*
         * Undocumented X/Y flags:
         *
         * register operand -> source value
         * (HL)             -> high byte of address
         */
        if (memory_operand) {
            flags |=
                (uint8_t)(
                    (get_hl(cpu) >> 8) &
                    (
                        Z80_FLAG_X |
                        Z80_FLAG_Y
                    )
                );
        } else {
            flags |=
                value &
                (
                    Z80_FLAG_X |
                    Z80_FLAG_Y
                );
        }

        cpu->f = flags;
        cpu->q = flags;

        return finish_step(
            cpu,
            memory_operand
                ? 12u
                : 8u,
            true
        );
    }

    /*
     * 10xxxxxx:
     * RES b,r
     */
    if (group == 2u) {
        const uint8_t result =
            (uint8_t)(
                value &
                (uint8_t)~(
                    (uint8_t)(
                        1u << operation
                    )
                )
            );

        write_register_code(
            cpu,
            code,
            result
        );

        return finish_step(
            cpu,
            memory_operand
                ? 15u
                : 8u,
            true
        );
    }

    /*
     * 11xxxxxx:
     * SET b,r
     */
    {
        const uint8_t result =
            (uint8_t)(
                value |
                (uint8_t)(
                    1u << operation
                )
            );

        write_register_code(
            cpu,
            code,
            result
        );

        return finish_step(
            cpu,
            memory_operand
                ? 15u
                : 8u,
            true
        );
    }
}

static void alu_adc_hl(
    Z80 *cpu,
    uint16_t value
) {
    const uint16_t hl =
        get_hl(cpu);

    const uint16_t carry =
        (cpu->f & Z80_FLAG_C)
            ? 1u
            : 0u;

    const uint32_t wide_result =
        (uint32_t)hl +
        (uint32_t)value +
        (uint32_t)carry;

    const uint16_t result =
        (uint16_t)wide_result;

    uint8_t flags =
        (uint8_t)(
            (result >> 8) &
            (
                Z80_FLAG_S |
                Z80_FLAG_X |
                Z80_FLAG_Y
            )
        );

    if (result == 0u) {
        flags |= Z80_FLAG_Z;
    }

    if (
        (
            (hl & 0x0fffu) +
            (value & 0x0fffu) +
            carry
        ) >
        0x0fffu
    ) {
        flags |= Z80_FLAG_H;
    }

    if (
        (
            (~(hl ^ value)) &
            (hl ^ result) &
            0x8000u
        ) != 0u
    ) {
        flags |= Z80_FLAG_PV;
    }

    if (wide_result > 0xffffu) {
        flags |= Z80_FLAG_C;
    }

    cpu->h =
        (uint8_t)(result >> 8);

    cpu->l =
        (uint8_t)result;

    cpu->f = flags;
    cpu->q = flags;
}

static void alu_sbc_hl(
    Z80 *cpu,
    uint16_t value
) {
    const uint16_t hl =
        get_hl(cpu);

    const uint16_t borrow =
        (cpu->f & Z80_FLAG_C)
            ? 1u
            : 0u;

    const uint32_t subtrahend =
        (uint32_t)value +
        (uint32_t)borrow;

    const uint16_t result =
        (uint16_t)(
            (uint32_t)hl -
            subtrahend
        );

    uint8_t flags =
        (uint8_t)(
            ((result >> 8) &
             (
                 Z80_FLAG_S |
                 Z80_FLAG_X |
                 Z80_FLAG_Y
             )) |
            Z80_FLAG_N
        );

    if (result == 0u) {
        flags |= Z80_FLAG_Z;
    }

    if (
        (
            (hl ^ value ^ result) &
            0x1000u
        ) != 0u
    ) {
        flags |= Z80_FLAG_H;
    }

    if (
        (
            (hl ^ value) &
            (hl ^ result) &
            0x8000u
        ) != 0u
    ) {
        flags |= Z80_FLAG_PV;
    }

    if ((uint32_t)hl < subtrahend) {
        flags |= Z80_FLAG_C;
    }

    cpu->h =
        (uint8_t)(result >> 8);

    cpu->l =
        (uint8_t)result;

    cpu->f = flags;
    cpu->q = flags;
}

static void alu_neg(
    Z80 *cpu
) {
    const uint8_t value =
        cpu->a;

    /*
     * NEG is equivalent to:
     *
     * A = 0 - A
     */
    cpu->a = 0u;

    alu_sub(
        cpu,
        value,
        0u
    );
}

static void load_a_from_interrupt_register(
    Z80 *cpu,
    uint8_t value
) {
    cpu->a = value;

    uint8_t flags =
        (cpu->f & Z80_FLAG_C) |
        result_szxy_flags(value);

    /*
     * For LD A,I and LD A,R, P/V reflects IFF2,
     * not the parity of A.
     */
    if (cpu->iff2) {
        flags |= Z80_FLAG_PV;
    }

    cpu->f = flags;
    cpu->q = flags;
}

static void block_io_flags(
    Z80 *cpu,
    uint8_t transferred,
    uint16_t k
) {
    /*
     * S/Z/X/Y follow the decremented B register.
     */
    uint8_t flags =
        cpu->b &
        (
            Z80_FLAG_S |
            Z80_FLAG_X |
            Z80_FLAG_Y
        );

    if (cpu->b == 0u) {
        flags |= Z80_FLAG_Z;
    }

    /*
     * N is copied from bit 7 of the transferred byte.
     */
    if ((transferred & 0x80u) != 0u) {
        flags |= Z80_FLAG_N;
    }

    /*
     * H and C are both set on the special block-I/O carry.
     */
    if (k > 0xffu) {
        flags |=
            Z80_FLAG_H |
            Z80_FLAG_C;
    }

    /*
     * P/V = parity((k & 7) XOR B).
     */
    if (
        parity_even(
            (uint8_t)(
                (k & 7u) ^
                cpu->b
            )
        )
    ) {
        flags |= Z80_FLAG_PV;
    }

    cpu->f = flags;
    cpu->q = flags;
}

static uint32_t execute_ed(
    Z80 *cpu
) {
    /*
     * ED's second byte is another M1 fetch.
     */
    const uint8_t opcode =
        memory_read(
            cpu,
            cpu->pc
        );

    cpu->pc++;
    increment_r(cpu);

    /*
     * IN r,(C)
     *
     * ED 40/48/50/58/60/68/70/78
     *
     * ED 70 discards the value but still updates flags.
     */
    if ((opcode & 0xc7u) == 0x40u) {
        const uint8_t code =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        const uint16_t port =
            (uint16_t)(
                ((uint16_t)cpu->b << 8) |
                cpu->c
            );

        const uint8_t value =
            io_read(
                cpu,
                port
            );

        if (code != 6u) {
            write_register_code(
                cpu,
                code,
                value
            );
        }

        uint8_t flags =
            (cpu->f & Z80_FLAG_C) |
            result_szxy_flags(value);

        if (parity_even(value)) {
            flags |= Z80_FLAG_PV;
        }

        cpu->f = flags;
        cpu->q = flags;

        cpu->wz =
            (uint16_t)(port + 1u);

        return finish_step(
            cpu,
            12u,
            true
        );
    }

    /*
     * OUT (C),r
     *
     * ED 41/49/51/59/61/69/71/79
     *
     * ED 71 outputs zero.
     */
    if ((opcode & 0xc7u) == 0x41u) {
        const uint8_t code =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        const uint16_t port =
            (uint16_t)(
                ((uint16_t)cpu->b << 8) |
                cpu->c
            );

        const uint8_t value =
            code == 6u
                ? 0u
                : read_register_code(
                    cpu,
                    code
                );

        io_write(
            cpu,
            port,
            value
        );

        cpu->wz =
            (uint16_t)(port + 1u);

        return finish_step(
            cpu,
            12u,
            true
        );
    }

    /*
     * SBC HL,rr
     *
     * ED 42/52/62/72
     */
    if ((opcode & 0xcfu) == 0x42u) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        alu_sbc_hl(
            cpu,
            get_register_pair(
                cpu,
                pair
            )
        );

        return finish_step(
            cpu,
            15u,
            true
        );
    }

    /*
     * ADC HL,rr
     *
     * ED 4A/5A/6A/7A
     */
    if ((opcode & 0xcfu) == 0x4au) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        alu_adc_hl(
            cpu,
            get_register_pair(
                cpu,
                pair
            )
        );

        return finish_step(
            cpu,
            15u,
            true
        );
    }

    /*
     * LD (nn),rr
     *
     * ED 43/53/63/73
     */
    if ((opcode & 0xcfu) == 0x43u) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        const uint16_t address =
            fetch_word(cpu);

        const uint16_t value =
            get_register_pair(
                cpu,
                pair
            );

        memory_write(
            cpu,
            address,
            (uint8_t)value
        );

        memory_write(
            cpu,
            (uint16_t)(address + 1u),
            (uint8_t)(value >> 8)
        );

        cpu->wz =
            (uint16_t)(address + 1u);

        return finish_step(
            cpu,
            20u,
            true
        );
    }

    /*
     * LD rr,(nn)
     *
     * ED 4B/5B/6B/7B
     */
    if ((opcode & 0xcfu) == 0x4bu) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        const uint16_t address =
            fetch_word(cpu);

        const uint8_t low =
            memory_read(
                cpu,
                address
            );

        const uint8_t high =
            memory_read(
                cpu,
                (uint16_t)(address + 1u)
            );

        set_register_pair(
            cpu,
            pair,
            (uint16_t)(
                ((uint16_t)high << 8) |
                low
            )
        );

        cpu->wz =
            (uint16_t)(address + 1u);

        return finish_step(
            cpu,
            20u,
            true
        );
    }

    /*
     * NEG
     *
     * ED 44 is canonical.
     * 4C/54/5C/64/6C/74/7C are aliases.
     */
    if ((opcode & 0xc7u) == 0x44u) {
        alu_neg(cpu);

        return finish_step(
            cpu,
            8u,
            true
        );
    }

    /*
     * RETN / RETI
     *
     * RETN:
     * 45/55/65/75
     *
     * RETI:
     * 4D/5D/6D/7D
     */
    if ((opcode & 0xc7u) == 0x45u) {
        const bool reti =
            (opcode & 0x08u) != 0u;

        const uint16_t target =
            pop_word(cpu);

        cpu->pc = target;
        cpu->wz = target;

        /*
         * Both RETN and RETI restore IFF1 from IFF2.
         */
        cpu->iff1 = cpu->iff2;

        if (
            reti &&
            cpu->bus.reti != 0
        ) {
            cpu->bus.reti(
                cpu->bus.context
            );
        }

        return finish_step(
            cpu,
            14u,
            true
        );
    }

    /*
     * IM 0
     */
    if (
        opcode == 0x46u ||
        opcode == 0x4eu ||
        opcode == 0x66u ||
        opcode == 0x6eu
    ) {
        cpu->interrupt_mode = 0u;

        return finish_step(
            cpu,
            8u,
            true
        );
    }

    /*
     * IM 1
     */
    if (
        opcode == 0x56u ||
        opcode == 0x76u
    ) {
        cpu->interrupt_mode = 1u;

        return finish_step(
            cpu,
            8u,
            true
        );
    }

    /*
     * IM 2
     */
    if (
        opcode == 0x5eu ||
        opcode == 0x7eu
    ) {
        cpu->interrupt_mode = 2u;

        return finish_step(
            cpu,
            8u,
            true
        );
    }

    /*
     * LD I,A
     */
    if (opcode == 0x47u) {
        cpu->i = cpu->a;

        return finish_step(
            cpu,
            9u,
            true
        );
    }

    /*
     * LD R,A
     */
    if (opcode == 0x4fu) {
        cpu->r = cpu->a;

        return finish_step(
            cpu,
            9u,
            true
        );
    }

    /*
     * LD A,I
     */
    if (opcode == 0x57u) {
        load_a_from_interrupt_register(
            cpu,
            cpu->i
        );

        return finish_step(
            cpu,
            9u,
            true
        );
    }

    /*
     * LD A,R
     */
    if (opcode == 0x5fu) {
        load_a_from_interrupt_register(
            cpu,
            cpu->r
        );

        return finish_step(
            cpu,
            9u,
            true
        );
    }

    /*
     * LDI / LDIR / LDD / LDDR
     *
     * A0 = LDI
     * A8 = LDD
     * B0 = LDIR
     * B8 = LDDR
     */
    if (
        opcode == 0xa0u ||
        opcode == 0xa8u ||
        opcode == 0xb0u ||
        opcode == 0xb8u
    ) {
        const bool decrement =
            (opcode & 0x08u) != 0u;

        const bool repeat =
            (opcode & 0x10u) != 0u;

        const uint16_t hl =
            get_hl(cpu);

        const uint16_t de =
            get_register_pair(
                cpu,
                1u
            );

        const uint16_t bc =
            get_register_pair(
                cpu,
                0u
            );

        const uint8_t value =
            memory_read(
                cpu,
                hl
            );

        memory_write(
            cpu,
            de,
            value
        );

        const uint16_t new_hl =
            decrement
                ? (uint16_t)(hl - 1u)
                : (uint16_t)(hl + 1u);

        const uint16_t new_de =
            decrement
                ? (uint16_t)(de - 1u)
                : (uint16_t)(de + 1u);

        const uint16_t new_bc =
            (uint16_t)(bc - 1u);

        set_register_pair(
            cpu,
            2u,
            new_hl
        );

        set_register_pair(
            cpu,
            1u,
            new_de
        );

        set_register_pair(
            cpu,
            0u,
            new_bc
        );

        /*
         * S, Z and C are preserved.
         * H and N are reset.
         * P/V indicates more bytes remain.
         *
         * Undocumented X/Y are derived from A + transferred byte.
         */
        const uint8_t sum =
            (uint8_t)(
                cpu->a +
                value
            );

        uint8_t flags =
            cpu->f &
            (
                Z80_FLAG_S |
                Z80_FLAG_Z |
                Z80_FLAG_C
            );

        if (new_bc != 0u) {
            flags |= Z80_FLAG_PV;
        }

        flags |=
            sum &
            Z80_FLAG_X;

        if ((sum & 0x02u) != 0u) {
            flags |= Z80_FLAG_Y;
        }

        cpu->f = flags;
        cpu->q = flags;

        if (
            repeat &&
            new_bc != 0u
        ) {
            /*
             * Repeat the complete ED instruction.
             */
            cpu->pc =
                (uint16_t)(cpu->pc - 2u);

            return finish_step(
                cpu,
                21u,
                true
            );
        }

        return finish_step(
            cpu,
            16u,
            true
        );
    }

    /*
     * CPI / CPIR / CPD / CPDR
     *
     * A1 = CPI
     * A9 = CPD
     * B1 = CPIR
     * B9 = CPDR
     */
    if (
        opcode == 0xa1u ||
        opcode == 0xa9u ||
        opcode == 0xb1u ||
        opcode == 0xb9u
    ) {
        const bool decrement =
            (opcode & 0x08u) != 0u;

        const bool repeat =
            (opcode & 0x10u) != 0u;

        const uint16_t hl =
            get_hl(cpu);

        const uint16_t bc =
            get_register_pair(
                cpu,
                0u
            );

        const uint8_t value =
            memory_read(
                cpu,
                hl
            );

        const uint8_t result =
            (uint8_t)(
                cpu->a -
                value
            );

        const bool half_carry =
            (
                (cpu->a ^
                 value ^
                 result) &
                0x10u
            ) != 0u;

        const uint16_t new_hl =
            decrement
                ? (uint16_t)(hl - 1u)
                : (uint16_t)(hl + 1u);

        const uint16_t new_bc =
            (uint16_t)(bc - 1u);

        set_register_pair(
            cpu,
            2u,
            new_hl
        );

        set_register_pair(
            cpu,
            0u,
            new_bc
        );

        uint8_t flags =
            (cpu->f & Z80_FLAG_C) |
            Z80_FLAG_N;

        if ((result & 0x80u) != 0u) {
            flags |= Z80_FLAG_S;
        }

        if (result == 0u) {
            flags |= Z80_FLAG_Z;
        }

        if (half_carry) {
            flags |= Z80_FLAG_H;
        }

        if (new_bc != 0u) {
            flags |= Z80_FLAG_PV;
        }

        /*
         * Undocumented X/Y use:
         *
         * A - (HL) - H
         */
        const uint8_t adjusted =
            (uint8_t)(
                result -
                (half_carry ? 1u : 0u)
            );

        flags |=
            adjusted &
            Z80_FLAG_X;

        if ((adjusted & 0x02u) != 0u) {
            flags |= Z80_FLAG_Y;
        }

        cpu->f = flags;
        cpu->q = flags;

        if (
            repeat &&
            new_bc != 0u &&
            result != 0u
        ) {
            cpu->pc =
                (uint16_t)(cpu->pc - 2u);

            return finish_step(
                cpu,
                21u,
                true
            );
        }

        return finish_step(
            cpu,
            16u,
            true
        );
    }

    /*
     * INI / IND / INIR / INDR
     *
     * A2 = INI
     * AA = IND
     * B2 = INIR
     * BA = INDR
     */
    if (
        opcode == 0xa2u ||
        opcode == 0xaau ||
        opcode == 0xb2u ||
        opcode == 0xbau
    ) {
        const bool decrement =
            (opcode & 0x08u) != 0u;

        const bool repeat =
            (opcode & 0x10u) != 0u;

        const uint16_t old_hl =
            get_hl(cpu);

        /*
         * For block input, B is decremented before BC
         * appears on the I/O address bus.
         */
        cpu->b--;

        const uint16_t port =
            (uint16_t)(
                ((uint16_t)cpu->b << 8) |
                cpu->c
            );

        const uint8_t value =
            io_read(
                cpu,
                port
            );

        memory_write(
            cpu,
            old_hl,
            value
        );

        const uint16_t new_hl =
            decrement
                ? (uint16_t)(old_hl - 1u)
                : (uint16_t)(old_hl + 1u);

        set_register_pair(
            cpu,
            2u,
            new_hl
        );

        const uint8_t adjusted_c =
            decrement
                ? (uint8_t)(cpu->c - 1u)
                : (uint8_t)(cpu->c + 1u);

        const uint16_t k =
            (uint16_t)value +
            (uint16_t)adjusted_c;

        block_io_flags(
            cpu,
            value,
            k
        );

        if (
            repeat &&
            cpu->b != 0u
        ) {
            cpu->pc =
                (uint16_t)(cpu->pc - 2u);

            cpu->wz =
                (uint16_t)(cpu->pc + 1u);

            return finish_step(
                cpu,
                21u,
                true
            );
        }

        cpu->wz =
            decrement
                ? (uint16_t)(port - 1u)
                : (uint16_t)(port + 1u);

        return finish_step(
            cpu,
            16u,
            true
        );
    }

    /*
     * OUTI / OUTD / OTIR / OTDR
     *
     * A3 = OUTI
     * AB = OUTD
     * B3 = OTIR
     * BB = OTDR
     */
    if (
        opcode == 0xa3u ||
        opcode == 0xabu ||
        opcode == 0xb3u ||
        opcode == 0xbbu
    ) {
        const bool decrement =
            (opcode & 0x08u) != 0u;

        const bool repeat =
            (opcode & 0x10u) != 0u;

        const uint16_t old_hl =
            get_hl(cpu);

        const uint16_t port =
            (uint16_t)(
                ((uint16_t)cpu->b << 8) |
                cpu->c
            );

        const uint8_t value =
            memory_read(
                cpu,
                old_hl
            );

        /*
         * Block output presents the original BC value.
         */
        io_write(
            cpu,
            port,
            value
        );

        cpu->b--;

        const uint16_t new_hl =
            decrement
                ? (uint16_t)(old_hl - 1u)
                : (uint16_t)(old_hl + 1u);

        set_register_pair(
            cpu,
            2u,
            new_hl
        );

        const uint8_t new_l =
            (uint8_t)new_hl;

        const uint16_t k =
            (uint16_t)value +
            (uint16_t)new_l;

        block_io_flags(
            cpu,
            value,
            k
        );

        if (
            repeat &&
            cpu->b != 0u
        ) {
            cpu->pc =
                (uint16_t)(cpu->pc - 2u);

            cpu->wz =
                (uint16_t)(cpu->pc + 1u);

            return finish_step(
                cpu,
                21u,
                true
            );
        }

        cpu->wz =
            decrement
                ? (uint16_t)(port - 1u)
                : (uint16_t)(port + 1u);

        return finish_step(
            cpu,
            16u,
            true
        );
    }

    /*
     * RRD
     *
     *      A low nibble
     *           |
     *           v
     * A <- low(HL) <- high(HL)
     */
    if (opcode == 0x67u) {
        const uint16_t address =
            get_hl(cpu);

        const uint8_t memory_value =
            memory_read(
                cpu,
                address
            );

        const uint8_t old_a =
            cpu->a;

        const uint8_t new_memory =
            (uint8_t)(
                ((old_a & 0x0fu) << 4) |
                (memory_value >> 4)
            );

        const uint8_t new_a =
            (uint8_t)(
                (old_a & 0xf0u) |
                (memory_value & 0x0fu)
            );

        memory_write(
            cpu,
            address,
            new_memory
        );

        cpu->a = new_a;

        uint8_t flags =
            (cpu->f & Z80_FLAG_C) |
            result_szxy_flags(new_a);

        if (parity_even(new_a)) {
            flags |= Z80_FLAG_PV;
        }

        cpu->f = flags;
        cpu->q = flags;

        cpu->wz =
            (uint16_t)(address + 1u);

        return finish_step(
            cpu,
            18u,
            true
        );
    }

    /*
     * RLD
     *
     * high(HL) <- low(HL) <- A low nibble
     */
    if (opcode == 0x6fu) {
        const uint16_t address =
            get_hl(cpu);

        const uint8_t memory_value =
            memory_read(
                cpu,
                address
            );

        const uint8_t old_a =
            cpu->a;

        const uint8_t new_memory =
            (uint8_t)(
                (memory_value << 4) |
                (old_a & 0x0fu)
            );

        const uint8_t new_a =
            (uint8_t)(
                (old_a & 0xf0u) |
                (memory_value >> 4)
            );

        memory_write(
            cpu,
            address,
            new_memory
        );

        cpu->a = new_a;

        uint8_t flags =
            (cpu->f & Z80_FLAG_C) |
            result_szxy_flags(new_a);

        if (parity_even(new_a)) {
            flags |= Z80_FLAG_PV;
        }

        cpu->f = flags;
        cpu->q = flags;

        cpu->wz =
            (uint16_t)(address + 1u);

        return finish_step(
            cpu,
            18u,
            true
        );
    }

    /*
     * Temporary development sentinel for ED opcodes
     * not implemented yet.
     */
    return 0u;
}

typedef struct {
    Z80Bus original_bus;
    uint16_t injected_address;
    uint8_t injected_opcode;
    bool injected_consumed;
} Z80InjectedBusContext;

static uint8_t injected_memory_read(
    void *context,
    uint16_t address
) {
    Z80InjectedBusContext *injected =
        (Z80InjectedBusContext *)context;

    if (
        !injected->injected_consumed &&
        address == injected->injected_address
    ) {
        injected->injected_consumed = true;
        return injected->injected_opcode;
    }

    if (injected->original_bus.memory_read == 0) {
        return 0xff;
    }

    return injected->original_bus.memory_read(
        injected->original_bus.context,
        address
    );
}

static void injected_memory_write(
    void *context,
    uint16_t address,
    uint8_t value
) {
    Z80InjectedBusContext *injected =
        (Z80InjectedBusContext *)context;

    if (injected->original_bus.memory_write == 0) {
        return;
    }

    injected->original_bus.memory_write(
        injected->original_bus.context,
        address,
        value
    );
}

static uint8_t injected_io_read(
    void *context,
    uint16_t port
) {
    Z80InjectedBusContext *injected =
        (Z80InjectedBusContext *)context;

    if (injected->original_bus.io_read == 0) {
        return 0xff;
    }

    return injected->original_bus.io_read(
        injected->original_bus.context,
        port
    );
}

static void injected_io_write(
    void *context,
    uint16_t port,
    uint8_t value
) {
    Z80InjectedBusContext *injected =
        (Z80InjectedBusContext *)context;

    if (injected->original_bus.io_write == 0) {
        return;
    }

    injected->original_bus.io_write(
        injected->original_bus.context,
        port,
        value
    );
}

static uint8_t injected_interrupt_acknowledge(
    void *context
) {
    Z80InjectedBusContext *injected =
        (Z80InjectedBusContext *)context;

    if (injected->original_bus.interrupt_acknowledge == 0) {
        return 0xff;
    }

    return injected->original_bus.interrupt_acknowledge(
        injected->original_bus.context
    );
}

static void injected_reti(
    void *context
) {
    Z80InjectedBusContext *injected =
        (Z80InjectedBusContext *)context;

    if (injected->original_bus.reti == 0) {
        return;
    }

    injected->original_bus.reti(
        injected->original_bus.context
    );
}

static uint16_t get_index(
    const Z80 *cpu,
    bool use_iy
) {
    return use_iy
        ? cpu->iy
        : cpu->ix;
}

static void set_index(
    Z80 *cpu,
    bool use_iy,
    uint16_t value
) {
    if (use_iy) {
        cpu->iy = value;
    } else {
        cpu->ix = value;
    }
}

static uint8_t read_index_register(
    Z80 *cpu,
    bool use_iy,
    uint8_t code
) {
    const uint16_t index =
        get_index(cpu, use_iy);

    switch (code & 7u) {
        case 0u: return cpu->b;
        case 1u: return cpu->c;
        case 2u: return cpu->d;
        case 3u: return cpu->e;
        case 4u: return (uint8_t)(index >> 8);
        case 5u: return (uint8_t)index;
        case 7u: return cpu->a;
        default: return 0xff;
    }
}

static void write_index_register(
    Z80 *cpu,
    bool use_iy,
    uint8_t code,
    uint8_t value
) {
    uint16_t index = get_index(cpu, use_iy);

    switch (code & 7u) {
        case 0u: cpu->b = value; break;
        case 1u: cpu->c = value; break;
        case 2u: cpu->d = value; break;
        case 3u: cpu->e = value; break;
        case 4u:
            index = (uint16_t)(((uint16_t)value << 8) | (index & 0x00ffu));
            set_index(cpu, use_iy, index);
            break;
        case 5u:
            index = (uint16_t)((index & 0xff00u) | value);
            set_index(cpu, use_iy, index);
            break;
        case 7u: cpu->a = value; break;
        default: break;
    }
}

static uint16_t indexed_address(
    const Z80 *cpu,
    bool use_iy,
    uint8_t displacement
) {
    const int16_t offset =
        displacement < 0x80u
            ? (int16_t)displacement
            : (int16_t)displacement - 0x100;

    return (uint16_t)((int32_t)get_index(cpu, use_iy) + offset);
}

static uint16_t get_index_pair(
    const Z80 *cpu,
    bool use_iy,
    uint8_t pair
) {
    if ((pair & 3u) == 2u) {
        return get_index(cpu, use_iy);
    }

    return get_register_pair(cpu, pair);
}

static void set_index_pair(
    Z80 *cpu,
    bool use_iy,
    uint8_t pair,
    uint16_t value
) {
    if ((pair & 3u) == 2u) {
        set_index(cpu, use_iy, value);
        return;
    }

    set_register_pair(cpu, pair, value);
}

static void alu_add_index(
    Z80 *cpu,
    bool use_iy,
    uint16_t value
) {
    const uint16_t index = get_index(cpu, use_iy);
    const uint32_t wide_result = (uint32_t)index + (uint32_t)value;
    const uint16_t result = (uint16_t)wide_result;

    uint8_t flags = cpu->f & (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_PV);
    flags |= (uint8_t)((result >> 8) & (Z80_FLAG_X | Z80_FLAG_Y));

    if (((index & 0x0fffu) + (value & 0x0fffu)) > 0x0fffu) {
        flags |= Z80_FLAG_H;
    }

    if (wide_result > 0xffffu) {
        flags |= Z80_FLAG_C;
    }

    set_index(cpu, use_iy, result);
    cpu->f = flags;
    cpu->q = flags;
    cpu->wz = (uint16_t)(index + 1u);
}

static void execute_index_alu(
    Z80 *cpu,
    uint8_t operation,
    uint8_t value
) {
    const uint8_t carry = (cpu->f & Z80_FLAG_C) ? 1u : 0u;

    switch (operation & 7u) {
        case 0u: alu_add(cpu, value, 0u); break;
        case 1u: alu_add(cpu, value, carry); break;
        case 2u: alu_sub(cpu, value, 0u); break;
        case 3u: alu_sub(cpu, value, carry); break;
        case 4u: alu_and(cpu, value); break;
        case 5u: alu_xor(cpu, value); break;
        case 6u: alu_or(cpu, value); break;
        case 7u: alu_cp(cpu, value); break;
        default: break;
    }
}

static uint32_t execute_index_cb(
    Z80 *cpu,
    bool use_iy
) {
    /*
     * DD/FD CB d op:
     *
     * DD/FD and CB are the two M1 fetches. The displacement and
     * final operation byte do not increment R.
     */
    const uint8_t displacement =
        fetch_byte(cpu);

    const uint8_t opcode =
        fetch_byte(cpu);

    const uint16_t address =
        indexed_address(
            cpu,
            use_iy,
            displacement
        );

    const uint8_t value =
        memory_read(
            cpu,
            address
        );

    const uint8_t group =
        (uint8_t)(opcode >> 6);

    const uint8_t operation =
        (uint8_t)(
            (opcode >> 3) &
            7u
        );

    const uint8_t destination =
        opcode & 7u;

    cpu->wz = address;

    if (group == 0u) {
        const uint8_t result =
            cb_rotate_shift(
                cpu,
                operation,
                value
            );

        memory_write(
            cpu,
            address,
            result
        );

        if (destination != 6u) {
            write_register_code(
                cpu,
                destination,
                result
            );
        }

        return finish_step(
            cpu,
            23u,
            true
        );
    }

    if (group == 1u) {
        const uint8_t mask =
            (uint8_t)(
                1u << operation
            );

        const bool bit_set =
            (value & mask) != 0u;

        uint8_t flags =
            (cpu->f & Z80_FLAG_C) |
            Z80_FLAG_H;

        if (!bit_set) {
            flags |=
                Z80_FLAG_Z |
                Z80_FLAG_PV;
        }

        if (
            operation == 7u &&
            bit_set
        ) {
            flags |= Z80_FLAG_S;
        }

        /*
         * For indexed BIT operations, undocumented X/Y are taken
         * from the high byte of the effective address.
         */
        flags |=
            (uint8_t)(
                (address >> 8) &
                (
                    Z80_FLAG_X |
                    Z80_FLAG_Y
                )
            );

        cpu->f = flags;
        cpu->q = flags;

        return finish_step(
            cpu,
            20u,
            true
        );
    }

    if (group == 2u) {
        const uint8_t result =
            (uint8_t)(
                value &
                (uint8_t)~(
                    (uint8_t)(
                        1u << operation
                    )
                )
            );

        memory_write(
            cpu,
            address,
            result
        );

        if (destination != 6u) {
            write_register_code(
                cpu,
                destination,
                result
            );
        }

        return finish_step(
            cpu,
            23u,
            true
        );
    }

    {
        const uint8_t result =
            (uint8_t)(
                value |
                (uint8_t)(
                    1u << operation
                )
            );

        memory_write(
            cpu,
            address,
            result
        );

        if (destination != 6u) {
            write_register_code(
                cpu,
                destination,
                result
            );
        }

        return finish_step(
            cpu,
            23u,
            true
        );
    }
}

static uint32_t execute_index(
    Z80 *cpu,
    bool use_iy
) {
    const uint8_t opcode = memory_read(cpu, cpu->pc);
    cpu->pc++;
    increment_r(cpu);

    if (opcode == 0xcbu) {
        return execute_index_cb(
            cpu,
            use_iy
        );
    }

    if ((opcode & 0xcfu) == 0x01u) {
        const uint8_t pair = (uint8_t)((opcode >> 4) & 3u);
        const uint16_t value = fetch_word(cpu);
        set_index_pair(cpu, use_iy, pair, value);
        return finish_step(cpu, 14u, true);
    }

    if ((opcode & 0xcfu) == 0x09u) {
        const uint8_t pair = (uint8_t)((opcode >> 4) & 3u);
        alu_add_index(cpu, use_iy, get_index_pair(cpu, use_iy, pair));
        return finish_step(cpu, 15u, true);
    }

    if ((opcode & 0xcfu) == 0x03u) {
        const uint8_t pair = (uint8_t)((opcode >> 4) & 3u);
        set_index_pair(cpu, use_iy, pair,
            (uint16_t)(get_index_pair(cpu, use_iy, pair) + 1u));
        return finish_step(cpu, 10u, true);
    }

    if ((opcode & 0xcfu) == 0x0bu) {
        const uint8_t pair = (uint8_t)((opcode >> 4) & 3u);
        set_index_pair(cpu, use_iy, pair,
            (uint16_t)(get_index_pair(cpu, use_iy, pair) - 1u));
        return finish_step(cpu, 10u, true);
    }

    if ((opcode & 0xc7u) == 0x04u) {
        const uint8_t code = (uint8_t)((opcode >> 3) & 7u);

        if (code == 6u) {
            const uint8_t displacement = fetch_byte(cpu);
            const uint16_t address = indexed_address(cpu, use_iy, displacement);
            const uint8_t result = alu_inc(cpu, memory_read(cpu, address));
            memory_write(cpu, address, result);
            cpu->wz = address;
            return finish_step(cpu, 23u, true);
        }

        const uint8_t value = read_index_register(cpu, use_iy, code);
        write_index_register(cpu, use_iy, code, alu_inc(cpu, value));
        return finish_step(cpu, 8u, true);
    }

    if ((opcode & 0xc7u) == 0x05u) {
        const uint8_t code = (uint8_t)((opcode >> 3) & 7u);

        if (code == 6u) {
            const uint8_t displacement = fetch_byte(cpu);
            const uint16_t address = indexed_address(cpu, use_iy, displacement);
            const uint8_t result = alu_dec(cpu, memory_read(cpu, address));
            memory_write(cpu, address, result);
            cpu->wz = address;
            return finish_step(cpu, 23u, true);
        }

        const uint8_t value = read_index_register(cpu, use_iy, code);
        write_index_register(cpu, use_iy, code, alu_dec(cpu, value));
        return finish_step(cpu, 8u, true);
    }

    if ((opcode & 0xc7u) == 0x06u) {
        const uint8_t code = (uint8_t)((opcode >> 3) & 7u);

        if (code == 6u) {
            const uint8_t displacement = fetch_byte(cpu);
            const uint8_t value = fetch_byte(cpu);
            const uint16_t address = indexed_address(cpu, use_iy, displacement);
            memory_write(cpu, address, value);
            cpu->wz = address;
            return finish_step(cpu, 19u, true);
        }

        write_index_register(cpu, use_iy, code, fetch_byte(cpu));
        return finish_step(cpu, 11u, true);
    }

    if (opcode >= 0x40u && opcode <= 0x7fu) {
        if (opcode == 0x76u) {
            cpu->halted = true;
            return finish_step(cpu, 8u, true);
        }

        const uint8_t destination = (uint8_t)((opcode >> 3) & 7u);
        const uint8_t source = opcode & 7u;

        if (source == 6u || destination == 6u) {
            const uint8_t displacement = fetch_byte(cpu);
            const uint16_t address = indexed_address(cpu, use_iy, displacement);

            if (source == 6u) {
                const uint8_t value = memory_read(cpu, address);
                write_register_code(cpu, destination, value);
            } else {
                const uint8_t value = read_register_code(cpu, source);
                memory_write(cpu, address, value);
            }

            cpu->wz = address;
            return finish_step(cpu, 19u, true);
        }

        const uint8_t value = read_index_register(cpu, use_iy, source);
        write_index_register(cpu, use_iy, destination, value);
        return finish_step(cpu, 8u, true);
    }

    if (opcode >= 0x80u && opcode <= 0xbfu) {
        const uint8_t operation = (uint8_t)((opcode >> 3) & 7u);
        const uint8_t source = opcode & 7u;

        if (source == 6u) {
            const uint8_t displacement = fetch_byte(cpu);
            const uint16_t address = indexed_address(cpu, use_iy, displacement);
            execute_index_alu(cpu, operation, memory_read(cpu, address));
            cpu->wz = address;
            return finish_step(cpu, 19u, true);
        }

        execute_index_alu(cpu, operation,
            read_index_register(cpu, use_iy, source));
        return finish_step(cpu, 8u, true);
    }

    if (opcode == 0x22u) {
        const uint16_t address = fetch_word(cpu);
        const uint16_t value = get_index(cpu, use_iy);
        memory_write(cpu, address, (uint8_t)value);
        memory_write(cpu, (uint16_t)(address + 1u), (uint8_t)(value >> 8));
        cpu->wz = (uint16_t)(address + 1u);
        return finish_step(cpu, 20u, true);
    }

    if (opcode == 0x2au) {
        const uint16_t address = fetch_word(cpu);
        const uint8_t low = memory_read(cpu, address);
        const uint8_t high = memory_read(cpu, (uint16_t)(address + 1u));
        set_index(cpu, use_iy, (uint16_t)(((uint16_t)high << 8) | low));
        cpu->wz = (uint16_t)(address + 1u);
        return finish_step(cpu, 20u, true);
    }

    if (opcode == 0xe1u) {
        set_index(cpu, use_iy, pop_word(cpu));
        return finish_step(cpu, 14u, true);
    }

    if (opcode == 0xe5u) {
        push_word(cpu, get_index(cpu, use_iy));
        return finish_step(cpu, 15u, true);
    }

    if (opcode == 0xe3u) {
        const uint16_t old_index = get_index(cpu, use_iy);
        const uint8_t low = memory_read(cpu, cpu->sp);
        const uint8_t high = memory_read(cpu, (uint16_t)(cpu->sp + 1u));

        memory_write(cpu, cpu->sp, (uint8_t)old_index);
        memory_write(cpu, (uint16_t)(cpu->sp + 1u), (uint8_t)(old_index >> 8));

        const uint16_t new_index = (uint16_t)(((uint16_t)high << 8) | low);
        set_index(cpu, use_iy, new_index);
        cpu->wz = new_index;
        return finish_step(cpu, 23u, true);
    }

    if (opcode == 0xe9u) {
        cpu->pc = get_index(cpu, use_iy);
        return finish_step(cpu, 8u, true);
    }

    if (opcode == 0xf9u) {
        cpu->sp = get_index(cpu, use_iy);
        return finish_step(cpu, 10u, true);
    }

    /*
     * All remaining opcodes are unaffected by DD/FD. Rewind the
     * already-fetched opcode and execute it through the normal decoder.
     *
     * This also handles repeated DD/FD prefixes and DD/FD followed by
     * ED. The ignored prefix itself costs four T-states and must not
     * decrement EI delay independently because it is part of the same
     * instruction.
     */
    cpu->pc--;
    decrement_r(cpu);

    const uint32_t cycles =
        z80_step(cpu);

    if (cycles == 0u) {
        return 0u;
    }

    cpu->total_t_states += 4u;

    return cycles + 4u;
}

static uint32_t service_nmi(
    Z80 *cpu
) {
    cpu->nmi_pending = false;
    cpu->halted = false;

    /*
     * NMI clears IFF1. IFF2 is preserved so RETN can
     * subsequently restore IFF1 from it.
     */
    cpu->iff1 = false;

    increment_r(cpu);

    push_word(
        cpu,
        cpu->pc
    );

    cpu->pc = 0x0066u;
    cpu->wz = 0x0066u;

    return finish_step(
        cpu,
        11u,
        false
    );
}

static uint32_t service_irq(
    Z80 *cpu
) {
    cpu->halted = false;

    cpu->iff1 = false;
    cpu->iff2 = false;

    if (cpu->interrupt_mode == 0u) {
        /*
         * In IM 0, the interrupting device supplies the first opcode
         * byte during the interrupt acknowledge M1 cycle. Any following
         * operand bytes are fetched from memory starting at the current
         * PC; the supplied opcode itself does not advance PC.
         */
        const uint8_t supplied_opcode =
            interrupt_acknowledge(cpu);

        const Z80Bus original_bus =
            cpu->bus;

        const uint16_t original_pc =
            cpu->pc;

        Z80InjectedBusContext injected = {
            .original_bus = original_bus,
            .injected_address =
                (uint16_t)(original_pc - 1u),
            .injected_opcode = supplied_opcode,
            .injected_consumed = false
        };

        cpu->bus = (Z80Bus){
            .context = &injected,
            .memory_read = injected_memory_read,
            .memory_write = injected_memory_write,
            .io_read = injected_io_read,
            .io_write = injected_io_write,
            .interrupt_acknowledge =
                injected_interrupt_acknowledge,
            .reti = injected_reti
        };

        cpu->pc =
            (uint16_t)(original_pc - 1u);

        const uint32_t instruction_cycles =
            z80_step(cpu);

        cpu->bus = original_bus;

        if (instruction_cycles == 0u) {
            cpu->pc = original_pc;
            return 0u;
        }

        /*
         * Interrupt acknowledge replaces the normal opcode fetch and
         * costs two extra T-states. For example, an IM 0 RST therefore
         * takes 13 T-states instead of the normal 11.
         */
        cpu->total_t_states += 2u;

        return instruction_cycles + 2u;
    }

    increment_r(cpu);

    if (cpu->interrupt_mode == 1u) {
        (void)interrupt_acknowledge(cpu);

        push_word(
            cpu,
            cpu->pc
        );

        cpu->pc = 0x0038u;
        cpu->wz = 0x0038u;

        return finish_step(
            cpu,
            13u,
            false
        );
    }

    {
        const uint8_t vector =
            interrupt_acknowledge(cpu);

        const uint16_t table_address =
            (uint16_t)(
                ((uint16_t)cpu->i << 8) |
                vector
            );

        const uint8_t low =
            memory_read(
                cpu,
                table_address
            );

        const uint8_t high =
            memory_read(
                cpu,
                (uint16_t)(table_address + 1u)
            );

        const uint16_t target =
            (uint16_t)(
                ((uint16_t)high << 8) |
                low
            );

        push_word(
            cpu,
            cpu->pc
        );

        cpu->pc = target;
        cpu->wz = target;

        return finish_step(
            cpu,
            19u,
            false
        );
    }
}

static bool opcode_is_supported(
    uint8_t opcode
) {
    if (
        (opcode >= 0x40u &&
         opcode <= 0x7fu) ||
        (opcode >= 0x80u &&
         opcode <= 0xbfu)
    ) {
        return true;
    }

    /*
     * INC/DEC r and INC/DEC rr.
     */
    if (
        (opcode & 0xc7u) == 0x04u ||
        (opcode & 0xc7u) == 0x05u ||
        (opcode & 0xcfu) == 0x03u ||
        (opcode & 0xcfu) == 0x0bu
    ) {
        return true;
    }

    /*
     * DJNZ and conditional JR.
     */
    if (
        opcode == 0x10u ||
        (opcode & 0xe7u) == 0x20u
    ) {
        return true;
    }

    /*
     * DD/FD index prefixes.
     */
    if (
        opcode == 0xddu ||
        opcode == 0xfdu
    ) {
        return true;
    }

    /*
     * ED-prefixed extended instruction space.
     */
    if (opcode == 0xedu) {
        return true;
    }

    /*
     * CB-prefixed rotate/shift/bit instructions.
     */
    if (opcode == 0xcbu) {
        return true;
    }

    /*
     * Accumulator rotates and flag operations.
     */
    if (
        opcode == 0x07u || /* RLCA */
        opcode == 0x0fu || /* RRCA */
        opcode == 0x17u || /* RLA */
        opcode == 0x1fu || /* RRA */
        opcode == 0x27u || /* DAA */
        opcode == 0x2fu || /* CPL */
        opcode == 0x37u || /* SCF */
        opcode == 0x3fu    /* CCF */
    ) {
        return true;
    }

    /*
     * Immediate ADC/SBC and IN A,(n).
     */
    if (
        opcode == 0xceu || /* ADC A,n */
        opcode == 0xdeu || /* SBC A,n */
        opcode == 0xdbu    /* IN A,(n) */
    ) {
        return true;
    }

    /*
     * Direct and indirect load family.
     */
    if (
        opcode == 0x02u || /* LD (BC),A */
        opcode == 0x0au || /* LD A,(BC) */
        opcode == 0x12u || /* LD (DE),A */
        opcode == 0x1au || /* LD A,(DE) */
        opcode == 0x22u || /* LD (nn),HL */
        opcode == 0x2au || /* LD HL,(nn) */
        opcode == 0x32u || /* LD (nn),A */
        opcode == 0x3au    /* LD A,(nn) */
    ) {
        return true;
    }

    /*
     * Register exchange and HL-indirect operations.
     */
    if (
        opcode == 0x08u || /* EX AF,AF' */
        opcode == 0xd9u || /* EXX */
        opcode == 0xe3u || /* EX (SP),HL */
        opcode == 0xe9u || /* JP (HL) */
        opcode == 0xebu || /* EX DE,HL */
        opcode == 0xf9u    /* LD SP,HL */
    ) {
        return true;
    }

    /*
     * ADD HL,rr:
     *
     * 09 19 29 39
     */
    if ((opcode & 0xcfu) == 0x09u) {
        return true;
    }

    /*
     * Conditional RET:
     *
     * C0 C8 D0 D8 E0 E8 F0 F8
     */
    if ((opcode & 0xc7u) == 0xc0u) {
        return true;
    }

    /*
     * Conditional JP:
     *
     * C2 CA D2 DA E2 EA F2 FA
     */
    if ((opcode & 0xc7u) == 0xc2u) {
        return true;
    }

    /*
     * Conditional CALL:
     *
     * C4 CC D4 DC E4 EC F4 FC
     */
    if ((opcode & 0xc7u) == 0xc4u) {
        return true;
    }

    /*
     * RST:
     *
     * C7 CF D7 DF E7 EF F7 FF
     */
    if ((opcode & 0xc7u) == 0xc7u) {
        return true;
    }

    /*
     * POP rr:
     * C1 D1 E1 F1
     */
    if ((opcode & 0xcfu) == 0xc1u) {
        return true;
    }

    /*
     * PUSH rr:
     * C5 D5 E5 F5
     */
    if ((opcode & 0xcfu) == 0xc5u) {
        return true;
    }

    switch (opcode) {
        case 0x00u:

        case 0x01u:
        case 0x06u:
        case 0x0eu:

        case 0x11u:
        case 0x16u:
        case 0x18u:
        case 0x1eu:

        case 0x21u:
        case 0x26u:
        case 0x2eu:

        case 0x31u:
        case 0x36u:
        case 0x3eu:

        case 0xc3u:
        case 0xc6u:
        case 0xc9u:
        case 0xcdu:

        case 0xd3u:
        case 0xd6u:

        case 0xe6u:
        case 0xeeu:

        case 0xf3u:
        case 0xf6u:
        case 0xfbu:
        case 0xfeu:
            return true;

        default:
            return false;
    }
}

void z80_init(
    Z80 *cpu,
    const Z80Bus *bus
) {
    *cpu = (Z80){0};

    if (bus != 0) {
        cpu->bus = *bus;
    }

    z80_reset(cpu);
}

void z80_reset(
    Z80 *cpu
) {
    const Z80Bus bus =
        cpu->bus;

    *cpu = (Z80){0};

    cpu->bus = bus;

    cpu->pc = 0x0000u;
    cpu->interrupt_mode = 0u;

    cpu->iff1 = false;
    cpu->iff2 = false;

    cpu->halted = false;
    cpu->irq_line = false;
    cpu->nmi_pending = false;

    cpu->ei_delay = 0u;
    cpu->total_t_states = 0u;
}

void z80_set_irq(
    Z80 *cpu,
    bool asserted
) {
    cpu->irq_line = asserted;
}

void z80_pulse_nmi(
    Z80 *cpu
) {
    cpu->nmi_pending = true;
}

uint32_t z80_step(
    Z80 *cpu
) {
    if (cpu->nmi_pending) {
        return service_nmi(cpu);
    }

    if (
        cpu->irq_line &&
        cpu->iff1 &&
        cpu->ei_delay == 0u
    ) {
        return service_irq(cpu);
    }

    if (cpu->halted) {
        increment_r(cpu);

        return finish_step(
            cpu,
            4u,
            false
        );
    }

    const uint8_t opcode =
        memory_read(
            cpu,
            cpu->pc
        );

    if (!opcode_is_supported(opcode)) {
        return 0u;
    }

    cpu->pc++;
    increment_r(cpu);

    /*
     * DD/FD-prefixed IX/IY instruction space.
     */
    if (
        opcode == 0xddu ||
        opcode == 0xfdu
    ) {
        return execute_index(
            cpu,
            opcode == 0xfdu
        );
    }

    /*
     * CB-prefixed instruction space.
     */
    if (opcode == 0xcbu) {
        return execute_cb(cpu);
    }

    /*
     * ED-prefixed instruction space.
     */
    if (opcode == 0xedu) {
        return execute_ed(cpu);
    }

    /*
     * ADD HL,rr
     *
     * BC, DE, HL, SP
     */
    if ((opcode & 0xcfu) == 0x09u) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        alu_add_hl(
            cpu,
            get_register_pair(
                cpu,
                pair
            )
        );

        return finish_step(
            cpu,
            11u,
            true
        );
    }

    /*
     * INC rr
     *
     * BC, DE, HL, SP
     */
    if ((opcode & 0xcfu) == 0x03u) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        set_register_pair(
            cpu,
            pair,
            (uint16_t)(
                get_register_pair(
                    cpu,
                    pair
                ) + 1u
            )
        );

        return finish_step(
            cpu,
            6u,
            true
        );
    }

    /*
     * DEC rr
     */
    if ((opcode & 0xcfu) == 0x0bu) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        set_register_pair(
            cpu,
            pair,
            (uint16_t)(
                get_register_pair(
                    cpu,
                    pair
                ) - 1u
            )
        );

        return finish_step(
            cpu,
            6u,
            true
        );
    }

    /*
     * INC r / INC (HL)
     */
    if ((opcode & 0xc7u) == 0x04u) {
        const uint8_t code =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        const uint8_t value =
            read_register_code(
                cpu,
                code
            );

        write_register_code(
            cpu,
            code,
            alu_inc(
                cpu,
                value
            )
        );

        return finish_step(
            cpu,
            code == 6u
                ? 11u
                : 4u,
            true
        );
    }

    /*
     * DEC r / DEC (HL)
     */
    if ((opcode & 0xc7u) == 0x05u) {
        const uint8_t code =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        const uint8_t value =
            read_register_code(
                cpu,
                code
            );

        write_register_code(
            cpu,
            code,
            alu_dec(
                cpu,
                value
            )
        );

        return finish_step(
            cpu,
            code == 6u
                ? 11u
                : 4u,
            true
        );
    }

    /*
     * DJNZ e
     */
    if (opcode == 0x10u) {
        const uint8_t displacement =
            fetch_byte(cpu);

        cpu->b--;

        if (cpu->b != 0u) {
            const int16_t offset =
                displacement < 0x80u
                    ? (int16_t)displacement
                    : (int16_t)displacement - 0x100;

            cpu->pc =
                (uint16_t)(
                    (int32_t)cpu->pc +
                    offset
                );

            cpu->wz = cpu->pc;

            return finish_step(
                cpu,
                13u,
                true
            );
        }

        return finish_step(
            cpu,
            8u,
            true
        );
    }

    /*
     * JR NZ,e
     * JR Z,e
     * JR NC,e
     * JR C,e
     */
    if ((opcode & 0xe7u) == 0x20u) {
        const uint8_t displacement =
            fetch_byte(cpu);

        const uint8_t condition =
            (uint8_t)(
                (opcode >> 3) &
                3u
            );

        bool take = false;

        switch (condition) {
            case 0u:
                take =
                    (cpu->f & Z80_FLAG_Z) == 0u;
                break;

            case 1u:
                take =
                    (cpu->f & Z80_FLAG_Z) != 0u;
                break;

            case 2u:
                take =
                    (cpu->f & Z80_FLAG_C) == 0u;
                break;

            case 3u:
                take =
                    (cpu->f & Z80_FLAG_C) != 0u;
                break;

            default:
                break;
        }

        if (take) {
            const int16_t offset =
                displacement < 0x80u
                    ? (int16_t)displacement
                    : (int16_t)displacement - 0x100;

            cpu->pc =
                (uint16_t)(
                    (int32_t)cpu->pc +
                    offset
                );

            cpu->wz = cpu->pc;

            return finish_step(
                cpu,
                12u,
                true
            );
        }

        return finish_step(
            cpu,
            7u,
            true
        );
    }

    /*
     * LD r,r'
     */
    if (
        opcode >= 0x40u &&
        opcode <= 0x7fu &&
        opcode != 0x76u
    ) {
        const uint8_t destination =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        const uint8_t source =
            opcode & 7u;

        const uint8_t value =
            read_register_code(
                cpu,
                source
            );

        write_register_code(
            cpu,
            destination,
            value
        );

        return finish_step(
            cpu,
            (
                source == 6u ||
                destination == 6u
            )
                ? 7u
                : 4u,
            true
        );
    }

    /*
     * ALU A,r / ALU A,(HL)
     */
    if (
        opcode >= 0x80u &&
        opcode <= 0xbfu
    ) {
        const uint8_t operation =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        const uint8_t source =
            opcode & 7u;

        const uint8_t value =
            read_register_code(
                cpu,
                source
            );

        const uint8_t old_carry =
            (cpu->f & Z80_FLAG_C)
                ? 1u
                : 0u;

        switch (operation) {
            case 0u:
                alu_add(
                    cpu,
                    value,
                    0u
                );
                break;

            case 1u:
                alu_add(
                    cpu,
                    value,
                    old_carry
                );
                break;

            case 2u:
                alu_sub(
                    cpu,
                    value,
                    0u
                );
                break;

            case 3u:
                alu_sub(
                    cpu,
                    value,
                    old_carry
                );
                break;

            case 4u:
                alu_and(
                    cpu,
                    value
                );
                break;

            case 5u:
                alu_xor(
                    cpu,
                    value
                );
                break;

            case 6u:
                alu_or(
                    cpu,
                    value
                );
                break;

            case 7u:
                alu_cp(
                    cpu,
                    value
                );
                break;

            default:
                break;
        }

        return finish_step(
            cpu,
            source == 6u
                ? 7u
                : 4u,
            true
        );
    }

    /*
     * RLCA
     */
    if (opcode == 0x07u) {
        rotate_accumulator_left_circular(cpu);

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * RRCA
     */
    if (opcode == 0x0fu) {
        rotate_accumulator_right_circular(cpu);

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * RLA
     */
    if (opcode == 0x17u) {
        rotate_accumulator_left(cpu);

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * RRA
     */
    if (opcode == 0x1fu) {
        rotate_accumulator_right(cpu);

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * DAA
     */
    if (opcode == 0x27u) {
        decimal_adjust_accumulator(cpu);

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * CPL
     */
    if (opcode == 0x2fu) {
        cpu->a =
            (uint8_t)~cpu->a;

        cpu->f =
            (uint8_t)(
                (cpu->f &
                 (
                     Z80_FLAG_S |
                     Z80_FLAG_Z |
                     Z80_FLAG_PV |
                     Z80_FLAG_C
                 )) |
                (cpu->a &
                 (
                     Z80_FLAG_X |
                     Z80_FLAG_Y
                 )) |
                Z80_FLAG_H |
                Z80_FLAG_N
            );

        cpu->q = cpu->f;

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * SCF
     */
    if (opcode == 0x37u) {
        cpu->f =
            (uint8_t)(
                (cpu->f &
                 (
                     Z80_FLAG_S |
                     Z80_FLAG_Z |
                     Z80_FLAG_PV
                 )) |
                (cpu->a &
                 (
                     Z80_FLAG_X |
                     Z80_FLAG_Y
                 )) |
                Z80_FLAG_C
            );

        cpu->q = cpu->f;

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * CCF
     */
    if (opcode == 0x3fu) {
        const bool old_carry =
            (cpu->f & Z80_FLAG_C) != 0u;

        cpu->f =
            (uint8_t)(
                (cpu->f &
                 (
                     Z80_FLAG_S |
                     Z80_FLAG_Z |
                     Z80_FLAG_PV
                 )) |
                (cpu->a &
                 (
                     Z80_FLAG_X |
                     Z80_FLAG_Y
                 )) |
                (old_carry
                    ? Z80_FLAG_H
                    : Z80_FLAG_C)
            );

        cpu->q = cpu->f;

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * ADC A,n
     */
    if (opcode == 0xceu) {
        const uint8_t carry =
            (cpu->f & Z80_FLAG_C)
                ? 1u
                : 0u;

        alu_add(
            cpu,
            fetch_byte(cpu),
            carry
        );

        return finish_step(
            cpu,
            7u,
            true
        );
    }

    /*
     * SBC A,n
     */
    if (opcode == 0xdeu) {
        const uint8_t borrow =
            (cpu->f & Z80_FLAG_C)
                ? 1u
                : 0u;

        alu_sub(
            cpu,
            fetch_byte(cpu),
            borrow
        );

        return finish_step(
            cpu,
            7u,
            true
        );
    }

    /*
     * IN A,(n)
     *
     * A supplies the upper half of the I/O address.
     */
    if (opcode == 0xdbu) {
        const uint8_t low_port =
            fetch_byte(cpu);

        const uint16_t port =
            (uint16_t)(
                ((uint16_t)cpu->a << 8) |
                low_port
            );

        cpu->a =
            io_read(
                cpu,
                port
            );

        cpu->wz =
            (uint16_t)(port + 1u);

        return finish_step(
            cpu,
            11u,
            true
        );
    }

    /*
     * LD (BC),A
     */
    if (opcode == 0x02u) {
        const uint16_t address =
            (uint16_t)(
                ((uint16_t)cpu->b << 8) |
                cpu->c
            );

        memory_write(
            cpu,
            address,
            cpu->a
        );

        return finish_step(
            cpu,
            7u,
            true
        );
    }

    /*
     * LD A,(BC)
     */
    if (opcode == 0x0au) {
        const uint16_t address =
            (uint16_t)(
                ((uint16_t)cpu->b << 8) |
                cpu->c
            );

        cpu->a =
            memory_read(
                cpu,
                address
            );

        return finish_step(
            cpu,
            7u,
            true
        );
    }

    /*
     * LD (DE),A
     */
    if (opcode == 0x12u) {
        const uint16_t address =
            (uint16_t)(
                ((uint16_t)cpu->d << 8) |
                cpu->e
            );

        memory_write(
            cpu,
            address,
            cpu->a
        );

        return finish_step(
            cpu,
            7u,
            true
        );
    }

    /*
     * LD A,(DE)
     */
    if (opcode == 0x1au) {
        const uint16_t address =
            (uint16_t)(
                ((uint16_t)cpu->d << 8) |
                cpu->e
            );

        cpu->a =
            memory_read(
                cpu,
                address
            );

        return finish_step(
            cpu,
            7u,
            true
        );
    }

    /*
     * LD (nn),HL
     */
    if (opcode == 0x22u) {
        const uint16_t address =
            fetch_word(cpu);

        memory_write(
            cpu,
            address,
            cpu->l
        );

        memory_write(
            cpu,
            (uint16_t)(address + 1u),
            cpu->h
        );

        cpu->wz =
            (uint16_t)(address + 1u);

        return finish_step(
            cpu,
            16u,
            true
        );
    }

    /*
     * LD HL,(nn)
     */
    if (opcode == 0x2au) {
        const uint16_t address =
            fetch_word(cpu);

        cpu->l =
            memory_read(
                cpu,
                address
            );

        cpu->h =
            memory_read(
                cpu,
                (uint16_t)(address + 1u)
            );

        cpu->wz =
            (uint16_t)(address + 1u);

        return finish_step(
            cpu,
            16u,
            true
        );
    }

    /*
     * LD (nn),A
     */
    if (opcode == 0x32u) {
        const uint16_t address =
            fetch_word(cpu);

        memory_write(
            cpu,
            address,
            cpu->a
        );

        cpu->wz =
            (uint16_t)(address + 1u);

        return finish_step(
            cpu,
            13u,
            true
        );
    }

    /*
     * LD A,(nn)
     */
    if (opcode == 0x3au) {
        const uint16_t address =
            fetch_word(cpu);

        cpu->a =
            memory_read(
                cpu,
                address
            );

        cpu->wz =
            (uint16_t)(address + 1u);

        return finish_step(
            cpu,
            13u,
            true
        );
    }

    /*
     * EX AF,AF'
     */
    if (opcode == 0x08u) {
        uint8_t temporary;

        temporary = cpu->a;
        cpu->a = cpu->a_alt;
        cpu->a_alt = temporary;

        temporary = cpu->f;
        cpu->f = cpu->f_alt;
        cpu->f_alt = temporary;

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * EXX
     */
    if (opcode == 0xd9u) {
        uint8_t temporary;

        temporary = cpu->b;
        cpu->b = cpu->b_alt;
        cpu->b_alt = temporary;

        temporary = cpu->c;
        cpu->c = cpu->c_alt;
        cpu->c_alt = temporary;

        temporary = cpu->d;
        cpu->d = cpu->d_alt;
        cpu->d_alt = temporary;

        temporary = cpu->e;
        cpu->e = cpu->e_alt;
        cpu->e_alt = temporary;

        temporary = cpu->h;
        cpu->h = cpu->h_alt;
        cpu->h_alt = temporary;

        temporary = cpu->l;
        cpu->l = cpu->l_alt;
        cpu->l_alt = temporary;

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * EX (SP),HL
     */
    if (opcode == 0xe3u) {
        const uint8_t memory_low =
            memory_read(
                cpu,
                cpu->sp
            );

        const uint8_t memory_high =
            memory_read(
                cpu,
                (uint16_t)(cpu->sp + 1u)
            );

        const uint8_t old_l =
            cpu->l;

        const uint8_t old_h =
            cpu->h;

        memory_write(
            cpu,
            cpu->sp,
            old_l
        );

        memory_write(
            cpu,
            (uint16_t)(cpu->sp + 1u),
            old_h
        );

        cpu->l = memory_low;
        cpu->h = memory_high;

        cpu->wz =
            get_hl(cpu);

        return finish_step(
            cpu,
            19u,
            true
        );
    }

    /*
     * JP (HL)
     */
    if (opcode == 0xe9u) {
        cpu->pc =
            get_hl(cpu);

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * EX DE,HL
     */
    if (opcode == 0xebu) {
        const uint8_t old_d =
            cpu->d;

        const uint8_t old_e =
            cpu->e;

        cpu->d = cpu->h;
        cpu->e = cpu->l;

        cpu->h = old_d;
        cpu->l = old_e;

        return finish_step(
            cpu,
            4u,
            true
        );
    }

    /*
     * LD SP,HL
     */
    if (opcode == 0xf9u) {
        cpu->sp =
            get_hl(cpu);

        return finish_step(
            cpu,
            6u,
            true
        );
    }

    /*
     * POP BC/DE/HL/AF
     *
     * bits 5..4 select the register pair.
     */
    if ((opcode & 0xcfu) == 0xc1u) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        const uint16_t value =
            pop_word(cpu);

        set_stack_pair(
            cpu,
            pair,
            value
        );

        return finish_step(
            cpu,
            10u,
            true
        );
    }

    /*
     * PUSH BC/DE/HL/AF
     */
    if ((opcode & 0xcfu) == 0xc5u) {
        const uint8_t pair =
            (uint8_t)(
                (opcode >> 4) &
                3u
            );

        push_word(
            cpu,
            get_stack_pair(
                cpu,
                pair
            )
        );

        return finish_step(
            cpu,
            11u,
            true
        );
    }

    /*
     * RET cc
     */
    if ((opcode & 0xc7u) == 0xc0u) {
        const uint8_t condition =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        if (
            condition_true(
                cpu,
                condition
            )
        ) {
            const uint16_t target =
                pop_word(cpu);

            cpu->pc = target;
            cpu->wz = target;

            return finish_step(
                cpu,
                11u,
                true
            );
        }

        return finish_step(
            cpu,
            5u,
            true
        );
    }

    /*
     * JP cc,nn
     */
    if ((opcode & 0xc7u) == 0xc2u) {
        const uint8_t condition =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        const uint16_t target =
            fetch_word(cpu);

        cpu->wz = target;

        if (
            condition_true(
                cpu,
                condition
            )
        ) {
            cpu->pc = target;
        }

        return finish_step(
            cpu,
            10u,
            true
        );
    }

    /*
     * CALL cc,nn
     */
    if ((opcode & 0xc7u) == 0xc4u) {
        const uint8_t condition =
            (uint8_t)(
                (opcode >> 3) &
                7u
            );

        const uint16_t target =
            fetch_word(cpu);

        cpu->wz = target;

        if (
            condition_true(
                cpu,
                condition
            )
        ) {
            push_word(
                cpu,
                cpu->pc
            );

            cpu->pc = target;

            return finish_step(
                cpu,
                17u,
                true
            );
        }

        return finish_step(
            cpu,
            10u,
            true
        );
    }

    /*
     * RST p
     */
    if ((opcode & 0xc7u) == 0xc7u) {
        const uint16_t target =
            (uint16_t)(
                opcode &
                0x38u
            );

        push_word(
            cpu,
            cpu->pc
        );

        cpu->pc = target;
        cpu->wz = target;

        return finish_step(
            cpu,
            11u,
            true
        );
    }

    switch (opcode) {
        case 0x00u:
            return finish_step(
                cpu,
                4u,
                true
            );

        case 0x01u:
            {
                const uint16_t value =
                    fetch_word(cpu);

                cpu->b =
                    (uint8_t)(value >> 8);

                cpu->c =
                    (uint8_t)value;

                return finish_step(
                    cpu,
                    10u,
                    true
                );
            }

        case 0x06u:
            cpu->b =
                fetch_byte(cpu);

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0x0eu:
            cpu->c =
                fetch_byte(cpu);

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0x11u:
            {
                const uint16_t value =
                    fetch_word(cpu);

                cpu->d =
                    (uint8_t)(value >> 8);

                cpu->e =
                    (uint8_t)value;

                return finish_step(
                    cpu,
                    10u,
                    true
                );
            }

        case 0x16u:
            cpu->d =
                fetch_byte(cpu);

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0x18u:
            {
                const uint8_t displacement =
                    fetch_byte(cpu);

                const int16_t offset =
                    displacement < 0x80u
                        ? (int16_t)displacement
                        : (int16_t)displacement - 0x100;

                cpu->pc =
                    (uint16_t)(
                        (int32_t)cpu->pc +
                        offset
                    );

                cpu->wz = cpu->pc;

                return finish_step(
                    cpu,
                    12u,
                    true
                );
            }

        case 0x1eu:
            cpu->e =
                fetch_byte(cpu);

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0x21u:
            {
                const uint16_t value =
                    fetch_word(cpu);

                cpu->h =
                    (uint8_t)(value >> 8);

                cpu->l =
                    (uint8_t)value;

                return finish_step(
                    cpu,
                    10u,
                    true
                );
            }

        case 0x26u:
            cpu->h =
                fetch_byte(cpu);

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0x2eu:
            cpu->l =
                fetch_byte(cpu);

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0x31u:
            cpu->sp =
                fetch_word(cpu);

            return finish_step(
                cpu,
                10u,
                true
            );

        case 0x36u:
            {
                const uint8_t value =
                    fetch_byte(cpu);

                memory_write(
                    cpu,
                    get_hl(cpu),
                    value
                );

                return finish_step(
                    cpu,
                    10u,
                    true
                );
            }

        case 0x3eu:
            cpu->a =
                fetch_byte(cpu);

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0x76u:
            cpu->halted = true;

            return finish_step(
                cpu,
                4u,
                true
            );

        case 0xc3u:
            {
                const uint16_t target =
                    fetch_word(cpu);

                cpu->pc = target;
                cpu->wz = target;

                return finish_step(
                    cpu,
                    10u,
                    true
                );
            }

        case 0xc6u:
            alu_add(
                cpu,
                fetch_byte(cpu),
                0u
            );

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0xc9u:
            {
                const uint16_t target =
                    pop_word(cpu);

                cpu->pc = target;
                cpu->wz = target;

                return finish_step(
                    cpu,
                    10u,
                    true
                );
            }

        case 0xcdu:
            {
                const uint16_t target =
                    fetch_word(cpu);

                push_word(
                    cpu,
                    cpu->pc
                );

                cpu->pc = target;
                cpu->wz = target;

                return finish_step(
                    cpu,
                    17u,
                    true
                );
            }

        case 0xd3u:
            {
                const uint8_t low_port =
                    fetch_byte(cpu);

                const uint16_t port =
                    (uint16_t)(
                        ((uint16_t)cpu->a << 8) |
                        low_port
                    );

                io_write(
                    cpu,
                    port,
                    cpu->a
                );

                cpu->wz =
                    (uint16_t)(port + 1u);

                return finish_step(
                    cpu,
                    11u,
                    true
                );
            }

        case 0xd6u:
            alu_sub(
                cpu,
                fetch_byte(cpu),
                0u
            );

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0xe6u:
            alu_and(
                cpu,
                fetch_byte(cpu)
            );

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0xeeu:
            alu_xor(
                cpu,
                fetch_byte(cpu)
            );

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0xf3u:
            cpu->iff1 = false;
            cpu->iff2 = false;
            cpu->ei_delay = 0u;

            return finish_step(
                cpu,
                4u,
                true
            );

        case 0xf6u:
            alu_or(
                cpu,
                fetch_byte(cpu)
            );

            return finish_step(
                cpu,
                7u,
                true
            );

        case 0xfbu:
            cpu->iff1 = true;
            cpu->iff2 = true;
            cpu->ei_delay = 2u;

            return finish_step(
                cpu,
                4u,
                true
            );

        case 0xfeu:
            alu_cp(
                cpu,
                fetch_byte(cpu)
            );

            return finish_step(
                cpu,
                7u,
                true
            );

        default:
            return 0u;
    }
}
