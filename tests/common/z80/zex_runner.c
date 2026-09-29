#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "z80.h"

enum {
    MEMORY_SIZE = 65536,
    PROGRAM_LOAD_ADDRESS = 0x0100,
    BDOS_ENTRY = 0x0005,
    BDOS_TARGET = 0xf000
};

typedef struct {
    uint8_t memory[MEMORY_SIZE];
} ZexHost;

static uint8_t memory_read(
    void *context,
    uint16_t address
) {
    ZexHost *host = context;
    return host->memory[address];
}

static void memory_write(
    void *context,
    uint16_t address,
    uint8_t value
) {
    ZexHost *host = context;
    host->memory[address] = value;
}

static uint8_t io_read(
    void *context,
    uint16_t port
) {
    (void)context;
    (void)port;

    return 0xff;
}

static void io_write(
    void *context,
    uint16_t port,
    uint8_t value
) {
    (void)context;
    (void)port;
    (void)value;
}

static uint8_t interrupt_acknowledge(
    void *context
) {
    (void)context;

    return 0xff;
}

static void reti(
    void *context
) {
    (void)context;
}

static uint16_t get_de(
    const Z80 *cpu
) {
    return (uint16_t)(
        ((uint16_t)cpu->d << 8) |
        cpu->e
    );
}

static uint16_t pop_return_address(
    Z80 *cpu,
    ZexHost *host
) {
    const uint8_t low =
        host->memory[cpu->sp];

    cpu->sp++;

    const uint8_t high =
        host->memory[cpu->sp];

    cpu->sp++;

    return (uint16_t)(
        ((uint16_t)high << 8) |
        low
    );
}

static bool handle_bdos(
    Z80 *cpu,
    ZexHost *host
) {
    switch (cpu->c) {
        /*
         * CP/M BDOS function 2:
         * console output, character in E.
         */
        case 2:
            fputc(
                cpu->e,
                stdout
            );
            break;

        /*
         * CP/M BDOS function 9:
         * print string at DE, terminated by '$'.
         */
        case 9:
            {
                uint16_t address =
                    get_de(cpu);

                bool terminated = false;

                for (
                    uint32_t count = 0;
                    count < MEMORY_SIZE;
                    count++
                ) {
                    const uint8_t character =
                        host->memory[address];

                    address++;

                    if (character == '$') {
                        terminated = true;
                        break;
                    }

                    fputc(
                        character,
                        stdout
                    );
                }

                if (!terminated) {
                    fprintf(
                        stderr,
                        "\nBDOS function 9 string "
                        "has no '$' terminator\n"
                    );

                    return false;
                }
            }
            break;

        default:
            fprintf(
                stderr,
                "\nUnsupported BDOS function: %u\n",
                cpu->c
            );

            return false;
    }

    fflush(stdout);

    /*
     * We intercept CALL 0005h at its destination.
     * Emulate the RET that the real BDOS would eventually execute.
     */
    cpu->pc =
        pop_return_address(
            cpu,
            host
        );

    return true;
}

static bool load_program(
    ZexHost *host,
    const char *filename
) {
    FILE *file =
        fopen(
            filename,
            "rb"
        );

    if (file == NULL) {
        perror(filename);
        return false;
    }

    if (
        fseek(
            file,
            0,
            SEEK_END
        ) != 0
    ) {
        perror("fseek");
        fclose(file);
        return false;
    }

    const long size =
        ftell(file);

    if (size < 0) {
        perror("ftell");
        fclose(file);
        return false;
    }

    if (
        (unsigned long)size >
        MEMORY_SIZE -
        PROGRAM_LOAD_ADDRESS
    ) {
        fprintf(
            stderr,
            "Program is too large: %ld bytes\n",
            size
        );

        fclose(file);
        return false;
    }

    rewind(file);

    const size_t read =
        fread(
            &host->memory[
                PROGRAM_LOAD_ADDRESS
            ],
            1,
            (size_t)size,
            file
        );

    fclose(file);

    if (read != (size_t)size) {
        fprintf(
            stderr,
            "Could not read complete program\n"
        );

        return false;
    }

    return true;
}

static void dump_failure(
    const Z80 *cpu,
    const ZexHost *host,
    uint16_t pc
) {
    fprintf(
        stderr,
        "\nZ80 execution stopped at PC=%04x\n",
        pc
    );

    fprintf(
        stderr,
        "bytes: %02x %02x %02x %02x\n",
        host->memory[pc],
        host->memory[(uint16_t)(pc + 1u)],
        host->memory[(uint16_t)(pc + 2u)],
        host->memory[(uint16_t)(pc + 3u)]
    );

    fprintf(
        stderr,
        "AF=%02x%02x BC=%02x%02x "
        "DE=%02x%02x HL=%02x%02x "
        "IX=%04x IY=%04x SP=%04x\n",
        cpu->a,
        cpu->f,
        cpu->b,
        cpu->c,
        cpu->d,
        cpu->e,
        cpu->h,
        cpu->l,
        cpu->ix,
        cpu->iy,
        cpu->sp
    );
}

int main(
    int argc,
    char **argv
) {
    if (argc != 2) {
        fprintf(
            stderr,
            "usage: %s zexdoc.cim\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }

    ZexHost host = {0};

    if (
        !load_program(
            &host,
            argv[1]
        )
    ) {
        return EXIT_FAILURE;
    }

    /*
     * Minimal CP/M page-zero environment.
     *
     * 0005h = JP F000h
     *
     * ZEXDOC reads the word at 0006h and uses it as its initial
     * stack pointer, exactly as it would use the CP/M BDOS address.
     */
    host.memory[0x0005] = 0xc3;
    host.memory[0x0006] =
        (uint8_t)BDOS_TARGET;
    host.memory[0x0007] =
        (uint8_t)(BDOS_TARGET >> 8);

    host.memory[BDOS_TARGET] = 0xc9;

    const Z80Bus bus = {
        .context = &host,
        .memory_read = memory_read,
        .memory_write = memory_write,
        .io_read = io_read,
        .io_write = io_write,
        .interrupt_acknowledge =
            interrupt_acknowledge,
        .reti = reti
    };

    Z80 cpu;

    z80_init(
        &cpu,
        &bus
    );

    cpu.pc =
        PROGRAM_LOAD_ADDRESS;

    uint64_t instructions = 0;

    for (;;) {
        /*
         * JP 0000h is CP/M warm boot and means the test is done.
         */
        if (cpu.pc == 0x0000u) {
            printf(
                "\nZEX finished after %llu instructions\n",
                (unsigned long long)instructions
            );

            return EXIT_SUCCESS;
        }

        if (cpu.pc == BDOS_ENTRY) {
            if (
                !handle_bdos(
                    &cpu,
                    &host
                )
            ) {
                return EXIT_FAILURE;
            }

            continue;
        }

        const uint16_t pc =
            cpu.pc;

        const uint32_t cycles =
            z80_step(
                &cpu
            );

        if (cycles == 0u) {
            dump_failure(
                &cpu,
                &host,
                pc
            );

            return EXIT_FAILURE;
        }

        instructions++;
    }
}
