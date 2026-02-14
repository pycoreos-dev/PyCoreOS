#ifndef KERNEL_RELEASE_H
#define KERNEL_RELEASE_H

#ifdef __cplusplus
extern "C" {
#endif

#define PYCOREOS_VERSION "0.1.0-beta.1"
#define PYCOREOS_CHANNEL "public-beta"
#define PYCOREOS_CODENAME "first-light"

const char* pycoreos_version(void);
const char* pycoreos_channel(void);
const char* pycoreos_codename(void);
const char* pycoreos_build_stamp(void);

#ifdef __cplusplus
}
#endif

#endif
