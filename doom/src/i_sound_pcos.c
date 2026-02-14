#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "i_sound.h"
#include "sounds.h"
#include "doom/libc_shim.h"

#ifdef SNDSERV
FILE* sndserver = (FILE*)0;
char* sndserver_filename = (char*)"";
#endif

static int s_next_song_handle = 1;

void I_InitSound(void) {
}

void I_UpdateSound(void) {
}

void I_SubmitSound(void) {
}

void I_ShutdownSound(void) {
}

void I_SetChannels(void) {
}

int I_GetSfxLumpNum(sfxinfo_t* sfxinfo) {
    (void)sfxinfo;
    return -1;
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority) {
    (void)id;
    (void)vol;
    (void)sep;
    (void)pitch;
    (void)priority;
    return 0;
}

void I_StopSound(int handle) {
    (void)handle;
}

int I_SoundIsPlaying(int handle) {
    (void)handle;
    return 0;
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch) {
    (void)handle;
    (void)vol;
    (void)sep;
    (void)pitch;
}

void I_InitMusic(void) {
}

void I_ShutdownMusic(void) {
}

void I_SetMusicVolume(int volume) {
    (void)volume;
}

void I_PauseSong(int handle) {
    (void)handle;
}

void I_ResumeSong(int handle) {
    (void)handle;
}

int I_RegisterSong(void* data) {
    (void)data;
    int handle = s_next_song_handle;
    ++s_next_song_handle;
    if (s_next_song_handle <= 0) {
        s_next_song_handle = 1;
    }
    return handle;
}

void I_PlaySong(int handle, int looping) {
    (void)handle;
    (void)looping;
}

void I_StopSong(int handle) {
    (void)handle;
}

void I_UnRegisterSong(int handle) {
    (void)handle;
}
