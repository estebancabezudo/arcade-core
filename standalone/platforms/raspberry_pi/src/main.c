#include "pacman.h"
#include "platform.h"

extern const PacmanStandaloneRomSet raspberry_pacman_roms;

static void halt_forever(void)
{
    for (;;) {
        __asm__ volatile("wfe");
    }
}

void kernel_main(void)
{
    if (!platform_init()) {
        halt_forever();
    }

    if (!pacman_standalone_init(&raspberry_pacman_roms)) {
        halt_forever();
    }

    pacman_standalone_run();
    halt_forever();
}
