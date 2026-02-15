/*
 * i_system_pcos.c — DOOM system interface for PyCoreOS
 *
 * Replaces the original i_system.c (which used malloc, gettimeofday, exit).
 * Uses PyCoreOS kernel APIs for timing and memory.
 */
#include "doomdef.h"
#include "doomtype.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "d_net.h"
#include "g_game.h"
#include "i_system.h"

/* PyCoreOS headers */
#include "kernel/serial.h"
#include "kernel/timing.h"
#include "gui/desktop.h"
#include "doom/libc_shim.h"

/* Zone memory — 6 MB static pool */
#define ZONE_SIZE (6 * 1024 * 1024)
static byte s_zone[ZONE_SIZE] __attribute__((aligned(16)));

int mb_used = 6;

void I_Tactile(int on, int off, int total) {
    (void)on; (void)off; (void)total;
}

ticcmd_t emptycmd;
ticcmd_t* I_BaseTiccmd(void) {
    return &emptycmd;
}

int I_GetHeapSize(void) {
    return mb_used * 1024 * 1024;
}

byte* I_ZoneBase(int* size) {
    *size = ZONE_SIZE;
    return s_zone;
}

/*
 * I_GetTime — returns time in 1/35th second tics (TICRATE = 35)
 * Uses rdtsc for monotonic timing.
 */
static inline uint64_t _sys_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t s_base_tsc = 0;
/* Approximate TSC ticks per tic (1/35 second).
 * At 3 GHz: 3000000000 / 35 ≈ 85714285.
 * We do a quick sleep-based estimate to avoid expensive PIT polling here. */
static uint64_t s_tsc_per_tic = 85714285ULL;
static int s_time_initialized = 0;

static void init_time(void) {
    if (s_time_initialized) return;
    s_base_tsc = _sys_rdtsc();
    {
        const uint64_t start = _sys_rdtsc();
        timing_sleep_ms(20);
        const uint64_t elapsed = _sys_rdtsc() - start;
        if (elapsed > 100000ULL) {
            const uint64_t est_per_tic = (elapsed * 50ULL) / TICRATE;
            if (est_per_tic > 100000ULL) {
                s_tsc_per_tic = est_per_tic;
            }
        }
    }

    s_time_initialized = 1;
    serial_write("[DOOM] Timer calibrated\n");
}

int I_GetTime(void) {
    if (!s_time_initialized) init_time();
    uint64_t elapsed = _sys_rdtsc() - s_base_tsc;
    return (int)(elapsed / s_tsc_per_tic);
}

void I_Init(void) {
    init_time();
    I_InitSound();
}

/* Flag to signal we should return to desktop instead of halting */
int doom_should_quit = 0;

void I_Quit(void) {
    D_QuitNetGame();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults();
    I_ShutdownGraphics();
    doom_should_quit = 1;
    serial_write("[DOOM] I_Quit called, returning to desktop\n");
    desktop_append_log("[DOOM] Quit requested");
}

void I_WaitVBL(int count) {
    if (count <= 0) count = 1;
    timing_sleep_ms((uint32_t)(count * (1000 / 70)));
}

void I_BeginRead(void) { }
void I_EndRead(void) { }

byte* I_AllocLow(int length) {
    byte* mem = (byte*)malloc((size_t)length);
    if (mem) memset(mem, 0, (size_t)length);
    return mem;
}

/* I_Error — print error and halt */
extern boolean demorecording;

void I_Error(char *error, ...) {
    va_list argptr;
    char buf[512];
    char line[544];

    va_start(argptr, error);
    vsnprintf(buf, sizeof(buf), error, argptr);
    va_end(argptr);

    serial_write("[DOOM ERROR] ");
    serial_write(buf);
    serial_write("\n");
    snprintf(line, sizeof(line), "[DOOM ERROR] %s", buf);
    desktop_append_log(line);

    if (demorecording)
        G_CheckDemoStatus();

    D_QuitNetGame();
    I_ShutdownSound();
    I_ShutdownMusic();
    I_ShutdownGraphics();

    doom_should_quit = 1;
    serial_write("[DOOM] I_Error requested quit to desktop\n");
    desktop_append_log("[DOOM] I_Error requested quit to desktop");
    return;
}

/* I_StartFrame — called at start of each frame (before tic processing) */
void I_StartFrame(void) {
    /* Nothing to do — keyboard polling happens in I_StartTic */
}
