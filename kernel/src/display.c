#include "kernel/display.h"

#include "drivers/framebuffer.h"

static bool s_ready = false;

void display_init(void) {
    s_ready = framebuffer_ready();
}

bool display_ready(void) {
    return s_ready;
}

uint32_t display_width(void) {
    return s_ready ? framebuffer_width() : 0;
}

uint32_t display_height(void) {
    return s_ready ? framebuffer_height() : 0;
}

uint32_t display_bpp(void) {
    return s_ready ? framebuffer_bpp() : 0;
}

uint32_t display_pitch(void) {
    return s_ready ? framebuffer_pitch() : 0;
}
