#ifndef ARCADE_CORE_STANDALONE_RASPBERRY_PI_H
#define ARCADE_CORE_STANDALONE_RASPBERRY_PI_H

#include <stdint.h>

#ifndef RPI_PERIPHERAL_BASE
#error "RPI_PERIPHERAL_BASE must be defined by the build"
#endif

enum {
    RPI_SCREEN_WIDTH = 1280,
    RPI_SCREEN_HEIGHT = 720,
    RPI_SCREEN_DEPTH = 32,

    RPI_GPIO_UP = 5,
    RPI_GPIO_DOWN = 6,
    RPI_GPIO_LEFT = 13,
    RPI_GPIO_RIGHT = 19,
    RPI_GPIO_COIN = 16,
    RPI_GPIO_START1 = 20,
    RPI_GPIO_START2 = 21,

    RPI_GPIO_AUDIO = 18,

    RPI_AUDIO_PDM_OVERSAMPLE = 32
};

static inline void rpi_dmb(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
}

static inline void rpi_dsb(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline uint32_t rpi_mmio_read(uintptr_t address)
{
    const volatile uint32_t *reg =
        (const volatile uint32_t *)address;
    const uint32_t value = *reg;
    rpi_dmb();
    return value;
}

static inline void rpi_mmio_write(uintptr_t address, uint32_t value)
{
    rpi_dmb();
    *(volatile uint32_t *)address = value;
    rpi_dsb();
}

#endif
