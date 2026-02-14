#include "kernel/serial.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

enum {
    kCom1 = 0x3F8,
};

static bool s_ready = false;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static bool tx_empty(void) {
    return (inb((uint16_t)(kCom1 + 5)) & 0x20U) != 0;
}

void serial_init(void) {
    outb((uint16_t)(kCom1 + 1), 0x00);
    outb((uint16_t)(kCom1 + 3), 0x80);
    outb((uint16_t)(kCom1 + 0), 0x03);
    outb((uint16_t)(kCom1 + 1), 0x00);
    outb((uint16_t)(kCom1 + 3), 0x03);
    outb((uint16_t)(kCom1 + 2), 0xC7);
    outb((uint16_t)(kCom1 + 4), 0x0B);
    s_ready = true;
}

void serial_write(const char* text) {
    if (!s_ready || text == NULL) {
        return;
    }

    for (size_t i = 0; text[i] != '\0'; ++i) {
        while (!tx_empty()) {
            __asm__ volatile("pause");
        }
        outb(kCom1, (uint8_t)text[i]);
    }
}
