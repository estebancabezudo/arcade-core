#include "platform.h"
#include "raspberry_pi.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MAILBOX_CHANNEL_PROPERTY = 8,

    GPIO_FUNCTION_INPUT = 0u,
    GPIO_FUNCTION_OUTPUT = 1u,

    FRAMEBUFFER_SCALE = 2,
    FRAMEBUFFER_CONTENT_WIDTH =
        PACMAN_STANDALONE_WIDTH * FRAMEBUFFER_SCALE,
    FRAMEBUFFER_CONTENT_HEIGHT =
        PACMAN_STANDALONE_HEIGHT * FRAMEBUFFER_SCALE
};

#define MAILBOX_STATUS_FULL UINT32_C(0x80000000)
#define MAILBOX_STATUS_EMPTY UINT32_C(0x40000000)

#define RPI_GPIO_BASE ((uintptr_t)RPI_PERIPHERAL_BASE + (uintptr_t)0x00200000u)
#define RPI_MAILBOX_BASE ((uintptr_t)RPI_PERIPHERAL_BASE + (uintptr_t)0x0000B880u)

#define GPIO_GPFSEL0 (RPI_GPIO_BASE + (uintptr_t)0x00u)
#define GPIO_GPSET0  (RPI_GPIO_BASE + (uintptr_t)0x1cu)
#define GPIO_GPCLR0  (RPI_GPIO_BASE + (uintptr_t)0x28u)
#define GPIO_GPLEV0  (RPI_GPIO_BASE + (uintptr_t)0x34u)

#define MAILBOX_READ   (RPI_MAILBOX_BASE + (uintptr_t)0x00u)
#define MAILBOX_STATUS (RPI_MAILBOX_BASE + (uintptr_t)0x18u)
#define MAILBOX_WRITE  (RPI_MAILBOX_BASE + (uintptr_t)0x20u)

static volatile uint32_t mailbox[36]
    __attribute__((aligned(16)));

static volatile uint32_t *framebuffer;
static uint32_t framebuffer_pitch;
static uint32_t framebuffer_width;
static uint32_t framebuffer_height;

static uint64_t audio_next_tick;
static uint64_t audio_tick_remainder;
static uint32_t audio_pdm_accumulator;
static bool audio_clock_started;

static uint64_t generic_counter(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(value));
    return value;
}

static uint64_t generic_frequency(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}

static void wait_until_counter(uint64_t deadline)
{
    while ((int64_t)(generic_counter() - deadline) < 0) {
        __asm__ volatile("yield");
    }
}

uint64_t platform_time_us(void)
{
    const uint64_t ticks = generic_counter();
    const uint64_t frequency = generic_frequency();

    if (frequency == 0u) {
        return 0u;
    }

    const uint64_t seconds = ticks / frequency;
    const uint64_t remainder = ticks % frequency;

    return
        seconds * UINT64_C(1000000) +
        (remainder * UINT64_C(1000000)) / frequency;
}

void platform_idle(void)
{
    __asm__ volatile("yield");
}

static void gpio_set_function(unsigned pin, unsigned function)
{
    const uintptr_t reg =
        GPIO_GPFSEL0 +
        (uintptr_t)(pin / 10u) * (uintptr_t)4u;
    const unsigned shift = (pin % 10u) * 3u;

    uint32_t value = rpi_mmio_read(reg);
    value &= ~(UINT32_C(7) << shift);
    value |= ((uint32_t)function & UINT32_C(7)) << shift;
    rpi_mmio_write(reg, value);
}

static void gpio_write(unsigned pin, bool high)
{
    const uint32_t mask = UINT32_C(1) << pin;
    rpi_mmio_write(high ? GPIO_GPSET0 : GPIO_GPCLR0, mask);
}

static bool gpio_active_low(unsigned pin)
{
    return
        (rpi_mmio_read(GPIO_GPLEV0) & (UINT32_C(1) << pin)) == 0u;
}

static bool mailbox_call(unsigned channel)
{
    const uint32_t message =
        ((uint32_t)(uintptr_t)mailbox & ~UINT32_C(0x0f)) |
        ((uint32_t)channel & UINT32_C(0x0f));

    while ((rpi_mmio_read(MAILBOX_STATUS) & MAILBOX_STATUS_FULL) != 0u) {
        __asm__ volatile("yield");
    }

    rpi_mmio_write(MAILBOX_WRITE, message);

    for (;;) {
        while ((rpi_mmio_read(MAILBOX_STATUS) & MAILBOX_STATUS_EMPTY) != 0u) {
            __asm__ volatile("yield");
        }

        if (rpi_mmio_read(MAILBOX_READ) == message) {
            return mailbox[1] == UINT32_C(0x80000000);
        }
    }
}

static bool framebuffer_init(void)
{
    unsigned i = 0u;

    mailbox[i++] = 0u;                 /* total size, filled below */
    mailbox[i++] = 0u;                 /* request */

    mailbox[i++] = UINT32_C(0x00048003); /* set physical size */
    mailbox[i++] = 8u;
    mailbox[i++] = 8u;
    mailbox[i++] = RPI_SCREEN_WIDTH;
    mailbox[i++] = RPI_SCREEN_HEIGHT;

    mailbox[i++] = UINT32_C(0x00048004); /* set virtual size */
    mailbox[i++] = 8u;
    mailbox[i++] = 8u;
    mailbox[i++] = RPI_SCREEN_WIDTH;
    mailbox[i++] = RPI_SCREEN_HEIGHT;

    mailbox[i++] = UINT32_C(0x00048005); /* set depth */
    mailbox[i++] = 4u;
    mailbox[i++] = 4u;
    mailbox[i++] = RPI_SCREEN_DEPTH;

    mailbox[i++] = UINT32_C(0x00048006); /* set pixel order: RGB */
    mailbox[i++] = 4u;
    mailbox[i++] = 4u;
    mailbox[i++] = 1u;

    mailbox[i++] = UINT32_C(0x00040001); /* allocate framebuffer */
    mailbox[i++] = 8u;
    mailbox[i++] = 8u;
    mailbox[i++] = 16u;                /* alignment */
    mailbox[i++] = 0u;

    mailbox[i++] = UINT32_C(0x00040008); /* get pitch */
    mailbox[i++] = 4u;
    mailbox[i++] = 4u;
    mailbox[i++] = 0u;

    mailbox[i++] = 0u;
    mailbox[0] = i * sizeof(uint32_t);

    if (!mailbox_call(MAILBOX_CHANNEL_PROPERTY)) {
        return false;
    }

    const uint32_t gpu_address = mailbox[23];
    const uint32_t size = mailbox[24];
    const uint32_t pitch = mailbox[28];

    if (gpu_address == 0u || size == 0u || pitch == 0u) {
        return false;
    }

    framebuffer =
        (volatile uint32_t *)(uintptr_t)(gpu_address & UINT32_C(0x3fffffff));
    framebuffer_pitch = pitch;
    framebuffer_width = RPI_SCREEN_WIDTH;
    framebuffer_height = RPI_SCREEN_HEIGHT;

    for (uint32_t y = 0; y < framebuffer_height; y++) {
        volatile uint32_t *row =
            (volatile uint32_t *)(
                (uintptr_t)framebuffer +
                (uintptr_t)y * framebuffer_pitch
            );

        for (uint32_t x = 0; x < framebuffer_width; x++) {
            row[x] = 0u;
        }
    }

    return true;
}

static uint32_t rgb565_to_xrgb8888(uint16_t pixel)
{
    const uint32_t r5 = (pixel >> 11) & UINT32_C(0x1f);
    const uint32_t g6 = (pixel >> 5) & UINT32_C(0x3f);
    const uint32_t b5 = pixel & UINT32_C(0x1f);

    const uint32_t r8 = (r5 << 3) | (r5 >> 2);
    const uint32_t g8 = (g6 << 2) | (g6 >> 4);
    const uint32_t b8 = (b5 << 3) | (b5 >> 2);

    return (r8 << 16) | (g8 << 8) | b8;
}

void platform_video_present(
    const uint16_t *pixels,
    size_t pixel_count
) {
    if (
        framebuffer == NULL ||
        pixels == NULL ||
        pixel_count !=
            (size_t)PACMAN_STANDALONE_WIDTH * PACMAN_STANDALONE_HEIGHT
    ) {
        return;
    }

    const uint32_t start_x =
        (framebuffer_width - FRAMEBUFFER_CONTENT_WIDTH) / 2u;
    const uint32_t start_y =
        (framebuffer_height - FRAMEBUFFER_CONTENT_HEIGHT) / 2u;

    for (uint32_t y = 0; y < PACMAN_STANDALONE_HEIGHT; y++) {
        volatile uint32_t *row0 =
            (volatile uint32_t *)(
                (uintptr_t)framebuffer +
                (uintptr_t)(start_y + y * 2u) * framebuffer_pitch
            ) + start_x;

        volatile uint32_t *row1 =
            (volatile uint32_t *)(
                (uintptr_t)framebuffer +
                (uintptr_t)(start_y + y * 2u + 1u) * framebuffer_pitch
            ) + start_x;

        const uint16_t *source =
            pixels + (size_t)y * PACMAN_STANDALONE_WIDTH;

        for (uint32_t x = 0; x < PACMAN_STANDALONE_WIDTH; x++) {
            const uint32_t color = rgb565_to_xrgb8888(source[x]);
            const uint32_t dx = x * 2u;

            row0[dx] = color;
            row0[dx + 1u] = color;
            row1[dx] = color;
            row1[dx + 1u] = color;
        }
    }

    rpi_dsb();
}

PacmanStandaloneInput platform_input_read(void)
{
    return (PacmanStandaloneInput){
        .up = gpio_active_low(RPI_GPIO_UP),
        .down = gpio_active_low(RPI_GPIO_DOWN),
        .left = gpio_active_low(RPI_GPIO_LEFT),
        .right = gpio_active_low(RPI_GPIO_RIGHT),
        .coin = gpio_active_low(RPI_GPIO_COIN),
        .start1 = gpio_active_low(RPI_GPIO_START1),
        .start2 = gpio_active_low(RPI_GPIO_START2)
    };
}

static void audio_wait_next_pdm_tick(void)
{
    const uint64_t frequency = generic_frequency();
    const uint64_t output_rate =
        (uint64_t)PACMAN_STANDALONE_AUDIO_RATE *
        RPI_AUDIO_PDM_OVERSAMPLE;

    const uint64_t whole = frequency / output_rate;
    const uint64_t remainder = frequency % output_rate;

    audio_next_tick += whole;
    audio_tick_remainder += remainder;

    if (audio_tick_remainder >= output_rate) {
        audio_next_tick++;
        audio_tick_remainder -= output_rate;
    }

    wait_until_counter(audio_next_tick);
}

void platform_audio_write(
    const int16_t *samples,
    size_t sample_count
) {
    if (samples == NULL || sample_count == 0u) {
        return;
    }

    if (!audio_clock_started) {
        audio_next_tick = generic_counter();
        audio_tick_remainder = 0u;
        audio_pdm_accumulator = 0u;
        audio_clock_started = true;
    }

    for (size_t i = 0; i < sample_count; i++) {
        const uint32_t level =
            (uint32_t)((int32_t)samples[i] + INT32_C(32768));

        for (unsigned n = 0; n < RPI_AUDIO_PDM_OVERSAMPLE; n++) {
            audio_pdm_accumulator += level;

            if (audio_pdm_accumulator >= UINT32_C(65536)) {
                audio_pdm_accumulator -= UINT32_C(65536);
                gpio_write(RPI_GPIO_AUDIO, true);
            } else {
                gpio_write(RPI_GPIO_AUDIO, false);
            }

            audio_wait_next_pdm_tick();
        }
    }
}

bool platform_init(void)
{
    /*
     * Inputs deliberately use external pull-up resistors. This avoids
     * BCM2837/BCM2711 differences in the internal pull configuration.
     */
    gpio_set_function(RPI_GPIO_UP, GPIO_FUNCTION_INPUT);
    gpio_set_function(RPI_GPIO_DOWN, GPIO_FUNCTION_INPUT);
    gpio_set_function(RPI_GPIO_LEFT, GPIO_FUNCTION_INPUT);
    gpio_set_function(RPI_GPIO_RIGHT, GPIO_FUNCTION_INPUT);
    gpio_set_function(RPI_GPIO_COIN, GPIO_FUNCTION_INPUT);
    gpio_set_function(RPI_GPIO_START1, GPIO_FUNCTION_INPUT);
    gpio_set_function(RPI_GPIO_START2, GPIO_FUNCTION_INPUT);

    gpio_set_function(RPI_GPIO_AUDIO, GPIO_FUNCTION_OUTPUT);
    gpio_write(RPI_GPIO_AUDIO, false);

    audio_clock_started = false;

    return framebuffer_init();
}
