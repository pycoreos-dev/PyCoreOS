/*
 * i_main_pcos.c — DOOM entry point for PyCoreOS
 *
 * Called from doom_bridge.c to launch the actual DOOM engine.
 * Sets up myargv/myargc and calls D_DoomMain().
 */
#include "doomdef.h"
#include "doomtype.h"
#include "m_argv.h"
#include "d_main.h"
#include "doom/libc_shim.h"
#include "kernel/serial.h"

extern int doom_should_quit;

void doom_main_entry(void) {
    serial_write("[DOOM] Starting DOOM engine...\n");

    /* Set up command line arguments for DOOM */
    /* -iwad DOOM1.WAD tells DOOM where to find the WAD */
    static char* argv[] = {
        "doom",
        "-iwad",
        "DOOM1.WAD",
        (char*)0
    };
    myargc = 3;
    myargv = argv;

    doom_should_quit = 0;

    /* This is the real deal — calls D_DoomMain from the original source */
    D_DoomMain();

    serial_write("[DOOM] D_DoomMain returned\n");
}
