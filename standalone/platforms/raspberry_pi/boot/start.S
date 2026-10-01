.section .text.boot
.align 7
.global _start
.type _start, %function

_start:
    /* Only core 0 runs the emulator. Park all other cores. */
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbz x0, 1f

0:
    wfe
    b 0b

1:
    /* Mask asynchronous exceptions; this target currently polls hardware. */
    msr daifset, #0xf

    ldr x0, =__stack_top
    mov sp, x0

    /* Clear .bss. */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
2:
    cmp x0, x1
    b.hs 3f
    str xzr, [x0], #8
    b 2b

3:
    bl kernel_main

4:
    wfe
    b 4b

.size _start, . - _start
