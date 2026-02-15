#include "drivers/keyboard.h"

#include <stdbool.h>
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static void drain_output(void) {
    for (int i = 0; i < 64; ++i) {
        if ((inb(0x64) & 0x01U) == 0) {
            break;
        }
        (void)inb(0x60);
    }
}

void keyboard_init(void) {
    drain_output();
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
    for (int i = 0; i < 16; ++i) {
        uint8_t status = inb(0x64);
        if ((status & 0x01U) == 0) {
            return false;
        }

        uint8_t sc = inb(0x60);
        if ((status & 0x20U) != 0) {
            /* AUX (mouse) byte: discard so mouse data cannot block keyboard input. */
            continue;
        }

        if (sc == 0xE0 || sc == 0xE1) {
            s_extended_prefix = true;
            continue;
        }

        bool released = (sc & 0x80U) != 0;
        uint8_t code = (uint8_t)(sc & 0x7FU);

        if (s_extended_prefix) {
            s_extended_prefix = false;
            continue;
        }

        if (code == 0x2AU || code == 0x36U) {
            s_shift = !released;
            continue;
        }

        if (!released && code == 0x3AU) {
            s_caps_lock = !s_caps_lock;
            continue;
        }

        if (released) {
            continue;
        }

        char c = scancode_to_ascii_us_qwerty(code, s_shift, s_caps_lock);
        if (c == 0) {
            continue;
        }

        *out = c;
        return true;
    }

    return false;
}
