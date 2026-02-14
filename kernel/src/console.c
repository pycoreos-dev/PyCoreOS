#include "kernel/console.h"

#include <stddef.h>
#include <stdint.h>

static volatile uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

static size_t s_row = 0;
static size_t s_col = 0;

static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void console_clear(uint8_t color) {
    for (size_t y = 0; y < VGA_HEIGHT; ++y) {
        for (size_t x = 0; x < VGA_WIDTH; ++x) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    s_row = 0;
    s_col = 0;
}

void console_init(void) {
    console_clear(0x1F);
}

void console_putc(char c, uint8_t color) {
    if (c == '\n') {
        s_col = 0;
        if (s_row + 1 < VGA_HEIGHT) {
            ++s_row;
        }
        return;
    }

    VGA_BUFFER[s_row * VGA_WIDTH + s_col] = vga_entry(c, color);
    ++s_col;
    if (s_col >= VGA_WIDTH) {
        s_col = 0;
        if (s_row + 1 < VGA_HEIGHT) {
            ++s_row;
        }
    }
}

void console_write(const char* s, uint8_t color) {
    while (*s) {
        console_putc(*s++, color);
    }
}
