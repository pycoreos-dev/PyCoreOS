#include "doom/doom_bridge.h"
#include "gui/desktop.h"
#include "kernel/serial.h"

extern void doom_main_entry(void);

static int s_initialized = 0;

void doom_bridge_init(void) {
    s_initialized = 1;
}

void doom_bridge_launch(void) {
    if (!s_initialized) {
        doom_bridge_init();
    }

    desktop_append_log("[DOOM] Launching id Software DOOM...");
    doom_main_entry();
    desktop_append_log("[DOOM] Returned to desktop");
    desktop_force_redraw();
}
