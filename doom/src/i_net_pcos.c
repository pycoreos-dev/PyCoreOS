/*
 * i_net_pcos.c — DOOM network interface for PyCoreOS (single-player only)
 *
 * Sets up doomcom for single-player mode. No network support.
 */
#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_net.h"
#include "i_system.h"
#include "i_net.h"
#include "doom/libc_shim.h"
#include "kernel/serial.h"

/* d_net.h defines doomcom_t with DOOMCOM_ID */
static doomcom_t s_doomcom;

void I_InitNetwork(void) {
    serial_write("[DOOM] Network: single-player mode\n");

    memset(&s_doomcom, 0, sizeof(s_doomcom));
    s_doomcom.id = DOOMCOM_ID;
    s_doomcom.ticdup = 1;
    s_doomcom.extratics = 0;
    s_doomcom.numnodes = 1;
    s_doomcom.numplayers = 1;
    s_doomcom.consoleplayer = 0;

    doomcom = &s_doomcom;
    netgame = false;
    deathmatch = false;
}

void I_NetCmd(void) {
    /* No network, nothing to do */
}
