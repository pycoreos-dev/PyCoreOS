/* Shim for <errno.h> */
#ifndef PCOS_ERRNO_SHIM_H
#define PCOS_ERRNO_SHIM_H
extern int errno;
#define ENOENT  2
#define EACCES 13
#define EEXIST 17
#endif
