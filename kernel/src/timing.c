#include "kernel/timing.h"

#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t pit_read_counter0(void) {
    outb(0x43U, 0x00U);
    const uint8_t lo = inb(0x40U);
    const uint8_t hi = inb(0x40U);
    return (uint16_t)(((uint16_t)hi << 8U) | (uint16_t)lo);
}

static uint32_t pit_elapsed_counts(uint16_t* inout_last) {
    const uint16_t cur = pit_read_counter0();
    const uint16_t prev = *inout_last;
    *inout_last = cur;
    if (prev >= cur) {
        return (uint32_t)(prev - cur);
    }
    return (uint32_t)prev + (65536U - (uint32_t)cur);
}

void timing_init_from_frame_cycles(uint32_t frame_cycles_60hz) {
    (void)frame_cycles_60hz;
}

void timing_sleep_ms(uint32_t ms) {
    if (ms == 0U) {
        return;
    }

    /* PIT input clock is 1,193,182 Hz: 1 ms = 1193 + 182/1000 counts. */
    uint16_t pit_last = pit_read_counter0();
    uint32_t pit_accum = 0U;
    uint32_t frac = 0U;
    uint32_t slept_ms = 0U;

    while (slept_ms < ms) {
        pit_accum += pit_elapsed_counts(&pit_last);

        while (slept_ms < ms) {
            uint32_t target = 1193U;
            uint32_t next_frac = frac + 182U;
            if (next_frac >= 1000U) {
                next_frac -= 1000U;
                ++target;
            }
            if (pit_accum < target) {
                break;
            }
            pit_accum -= target;
            frac = next_frac;
            ++slept_ms;
        }

        __asm__ volatile("pause");
    }
}
