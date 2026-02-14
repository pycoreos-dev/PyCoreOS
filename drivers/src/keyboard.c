#include "drivers/keyboard.h"

#include <stdbool.h>
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void keyboard_init(void) {
}

static bool s_shift = false;
static bool s_caps_lock = false;
static bool s_extended_prefix = false;

static char scancode_to_ascii_us_qwerty(uint8_t sc, bool shift, bool caps_lock) {
    static const char map[128] = {
        0,   27,  '1', '2', '3', '4', '5', '6',
        '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
        'o', 'p', '[', ']', '\n', 0,   'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
        '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', 0,   '*',
        0,   ' ',
    };

    static const char map_shift[128] = {
        0,   27,  '!', '@', '#', '$', '%', '^',
        '&', '*', '(', ')', '_', '+', '\b', '\t',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
        'O', 'P', '{', '}', '\n', 0,   'A', 'S',
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
        '\"', '~', 0,   '|', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', '<', '>', '?', 0,   '*',
        0,   ' ',
    };

    if (sc >= sizeof(map)) {
        return 0;
    }

    char base = map[sc];
    char shifted = map_shift[sc];
    if (base == 0) {
        return 0;
    }

    bool is_alpha = (base >= 'a' && base <= 'z');
    if (is_alpha) {
        bool uppercase = shift ^ caps_lock;
        return uppercase ? shifted : base;
    }

    return shift ? shifted : base;
}

bool keyboard_read_char(char* out) {
    uint8_t status = inb(0x64);
    if ((status & 0x01) == 0) {
        return false;
    }
    if ((status & 0x20) != 0) {
        return false;
    }

    uint8_t sc = inb(0x60);
    if (sc == 0xE0 || sc == 0xE1) {
        s_extended_prefix = true;
        return false;
    }

    bool released = (sc & 0x80) != 0;
    uint8_t code = (uint8_t)(sc & 0x7F);

    if (s_extended_prefix) {
        s_extended_prefix = false;
        return false;
    }

    if (code == 0x2A || code == 0x36) {
        s_shift = !released;
        return false;
    }

    if (!released && code == 0x3A) {
        s_caps_lock = !s_caps_lock;
        return false;
    }

    if (released) {
        return false;
    }

    char c = scancode_to_ascii_us_qwerty(code, s_shift, s_caps_lock);
    if (c == 0) {
        return false;
    }

    *out = c;
    return true;
}
