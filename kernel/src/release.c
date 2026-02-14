#include "kernel/release.h"

static const char kBuildStamp[] = __DATE__ " " __TIME__;

const char* pycoreos_version(void) {
    return PYCOREOS_VERSION;
}

const char* pycoreos_channel(void) {
    return PYCOREOS_CHANNEL;
}

const char* pycoreos_codename(void) {
    return PYCOREOS_CODENAME;
}

const char* pycoreos_build_stamp(void) {
    return kBuildStamp;
}
