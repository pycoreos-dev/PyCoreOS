#include "doom/doom_bridge.h"
#include "gui/desktop.h"
#include "kernel/filesystem.h"
#include "kernel/serial.h"

#include <stddef.h>
#include <stdint.h>

extern void doom_main_entry(void);

static int s_initialized = 0;

void doom_bridge_init(void) {
    s_initialized = 1;
}

void doom_bridge_launch(void) {
    if (!s_initialized) {
        doom_bridge_init();
    }

    const uint8_t* wad_data = NULL;
    size_t wad_size = 0;
    if (!fs_map_readonly("DOOM1.WAD", &wad_data, &wad_size)) {
        serial_write("[DOOM] Missing DOOM1.WAD in virtual filesystem\n");
        desktop_append_log("[DOOM] Missing DOOM1.WAD; cannot launch");
        desktop_force_redraw();
        return;
    }
    if (wad_size < 12U) {
        serial_write("[DOOM] DOOM1.WAD too small/invalid\n");
        desktop_append_log("[DOOM] DOOM1.WAD invalid (too small)");
        desktop_force_redraw();
        return;
    }
    if (!((wad_data[0] == 'I' || wad_data[0] == 'P') &&
          wad_data[1] == 'W' && wad_data[2] == 'A' && wad_data[3] == 'D')) {
        serial_write("[DOOM] DOOM1.WAD has invalid header\n");
        desktop_append_log("[DOOM] DOOM1.WAD invalid header");
        desktop_force_redraw();
        return;
    }

    serial_write("[DOOM] WAD preflight ok\n");
    desktop_append_log("[DOOM] Launching id Software DOOM...");
    doom_main_entry();
    desktop_append_log("[DOOM] Returned to desktop");
    desktop_force_redraw();
}
