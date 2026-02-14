/*
 * i_video_pcos.c — DOOM video interface for PyCoreOS
 *
 * Replaces the original i_video.c (which used X11/XShm).
 * Uses PyCoreOS framebuffer driver for display output.
 * Converts DOOM's 320x200 8-bit paletted framebuffer to ARGB8888
 * and blits it via framebuffer_present_argb8888().
 */
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "d_event.h"
#include "d_main.h"
#include "m_argv.h"

/* PyCoreOS headers */
#include "drivers/framebuffer.h"
#include "drivers/keyboard.h"
#include "kernel/serial.h"
#include "doom/libc_shim.h"

/* DOOM renders at 320x200 */
#define DOOM_WIDTH  320
#define DOOM_HEIGHT 200

/* ARGB8888 output buffer — upscaled to fit framebuffer */
/* Max supported: 1280x720, each DOOM pixel maps to 3x */
#define MAX_FB_WIDTH  1280
#define MAX_FB_HEIGHT 720

static uint32_t s_argb_buffer[MAX_FB_WIDTH * MAX_FB_HEIGHT];

/* Current palette: 256 entries, each entry is ARGB8888 */
static uint32_t s_palette[256];

/* DOOM uses extern screens[] — we provide screen 0 */
/* screens[0] is allocated by V_Init in v_video.c */

void I_InitGraphics(void) {
    serial_write("[DOOM] I_InitGraphics\n");
    /* Nothing special — framebuffer is already initialized by kernel */
}

void I_ShutdownGraphics(void) {
    serial_write("[DOOM] I_ShutdownGraphics\n");
}

void I_SetPalette(byte* palette) {
    /* DOOM provides 256 * 3 bytes (R, G, B) */
    for (int i = 0; i < 256; i++) {
        uint8_t r = palette[i * 3 + 0];
        uint8_t g = palette[i * 3 + 1];
        uint8_t b = palette[i * 3 + 2];
        s_palette[i] = (0xFF000000U) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

void I_UpdateNoBlit(void) {
    /* Nothing to do */
}

void I_FinishUpdate(void) {
    if (!framebuffer_ready()) return;

    /* Get framebuffer dimensions */
    uint32_t fb_w = framebuffer_width();
    uint32_t fb_h = framebuffer_height();
    if (fb_w == 0 || fb_h == 0) return;
    if (fb_w > MAX_FB_WIDTH) fb_w = MAX_FB_WIDTH;
    if (fb_h > MAX_FB_HEIGHT) fb_h = MAX_FB_HEIGHT;

    /* Calculate scale factors — integer scaling */
    uint32_t scale_x = fb_w / DOOM_WIDTH;
    uint32_t scale_y = fb_h / DOOM_HEIGHT;
    if (scale_x == 0) scale_x = 1;
    if (scale_y == 0) scale_y = 1;

    /* Use the smaller scale for uniform scaling */
    uint32_t scale = scale_x < scale_y ? scale_x : scale_y;

    uint32_t out_w = DOOM_WIDTH * scale;
    uint32_t out_h = DOOM_HEIGHT * scale;

    /* Center the image */
    uint32_t off_x = (fb_w - out_w) / 2;
    uint32_t off_y = (fb_h - out_h) / 2;

    /* Clear the border areas */
    memset(s_argb_buffer, 0, fb_w * fb_h * sizeof(uint32_t));

    /* Convert 8-bit indexed to ARGB8888 with scaling */
    byte* src = screens[0];
    if (!src) return;

    for (uint32_t sy = 0; sy < DOOM_HEIGHT; sy++) {
        for (uint32_t sx = 0; sx < DOOM_WIDTH; sx++) {
            uint32_t color = s_palette[src[sy * DOOM_WIDTH + sx]];
            /* Write scaled pixels */
            for (uint32_t dy = 0; dy < scale; dy++) {
                uint32_t out_y_pos = off_y + sy * scale + dy;
                if (out_y_pos >= fb_h) continue;
                uint32_t* row = &s_argb_buffer[out_y_pos * fb_w];
                for (uint32_t dx = 0; dx < scale; dx++) {
                    uint32_t out_x_pos = off_x + sx * scale + dx;
                    if (out_x_pos >= fb_w) continue;
                    row[out_x_pos] = color;
                }
            }
        }
    }

    /* Blit to framebuffer */
    framebuffer_present_argb8888(s_argb_buffer, fb_w);
}

void I_ReadScreen(byte* scr) {
    memcpy(scr, screens[0], DOOM_WIDTH * DOOM_HEIGHT);
}

/* ======================================================================
 * Keyboard input — I_StartTic
 *
 * DOOM expects keyboard events posted via D_PostEvent().
 * We poll the PS/2 keyboard directly and translate scancodes to DOOM keys.
 * ====================================================================== */

/*
 * Scancode-to-DOOM-key translation table.
 * DOOM Key codes are either ASCII or KEY_* from doomdef.h.
 */
static int scancode_to_doom_key(uint8_t sc) {
    switch (sc) {
        case 0x01: return KEY_ESCAPE;
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x0C: return KEY_MINUS;
        case 0x0D: return KEY_EQUALS;
        case 0x0E: return KEY_BACKSPACE;
        case 0x0F: return KEY_TAB;
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1A: return '[';
        case 0x1B: return ']';
        case 0x1C: return KEY_ENTER;
        case 0x1D: return KEY_RCTRL;
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x27: return ';';
        case 0x28: return '\'';
        case 0x29: return '`';
        case 0x2A: return KEY_RSHIFT; /* Left Shift */
        case 0x2B: return '\\';
        case 0x2C: return 'z';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';
        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '/';
        case 0x36: return KEY_RSHIFT; /* Right Shift */
        case 0x38: return KEY_RALT;   /* Alt */
        case 0x39: return ' ';
        case 0x3A: return 0; /* Caps Lock — ignore */
        /* F-keys */
        case 0x3B: return KEY_F1;
        case 0x3C: return KEY_F2;
        case 0x3D: return KEY_F3;
        case 0x3E: return KEY_F4;
        case 0x3F: return KEY_F5;
        case 0x40: return KEY_F6;
        case 0x41: return KEY_F7;
        case 0x42: return KEY_F8;
        case 0x43: return KEY_F9;
        case 0x44: return KEY_F10;
        case 0x57: return KEY_F11;
        case 0x58: return KEY_F12;
        case 0xE0: return 0; /* Extended prefix — handled separately */
        default: return 0;
    }
}

static int extended_scancode_to_doom_key(uint8_t sc) {
    switch (sc) {
        case 0x48: return KEY_UPARROW;
        case 0x50: return KEY_DOWNARROW;
        case 0x4B: return KEY_LEFTARROW;
        case 0x4D: return KEY_RIGHTARROW;
        case 0x1D: return KEY_RCTRL;
        case 0x38: return KEY_RALT;
        default: return 0;
    }
}

/* Inline port I/O */
static inline uint8_t _kb_inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/* Track extended key sequences */
static int s_extended = 0;

void I_StartTic(void) {
    event_t event;
    int budget = 16;

    while (budget-- > 0) {
        uint8_t status = _kb_inb(0x64);
        if ((status & 0x01) == 0) break;
        if ((status & 0x20) != 0) { _kb_inb(0x60); continue; } /* skip mouse data */

        uint8_t sc = _kb_inb(0x60);

        /* Handle extended prefix */
        if (sc == 0xE0) {
            s_extended = 1;
            continue;
        }
        if (sc == 0xE1) {
            /* Pause key — skip next two bytes */
            continue;
        }

        int released = (sc & 0x80) != 0;
        uint8_t code = sc & 0x7F;

        int doom_key;
        if (s_extended) {
            doom_key = extended_scancode_to_doom_key(code);
            s_extended = 0;
        } else {
            doom_key = scancode_to_doom_key(code);
        }

        if (doom_key == 0) continue;

        event.type = released ? ev_keyup : ev_keydown;
        event.data1 = doom_key;
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
    }
}
