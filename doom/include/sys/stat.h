/* Shim for <sys/stat.h> — provides a basic struct stat and fstat stub */
#ifndef PCOS_SYS_STAT_H
#define PCOS_SYS_STAT_H

#include "doom/libc_shim.h"

struct stat {
    unsigned long st_size;
    unsigned long st_mode;
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    unsigned long st_uid;
    unsigned long st_gid;
    long st_atime;
    long st_mtime;
    long st_ctime;
};

/* fstat stub — retrieve size from our in-memory fd table */
int fstat(int fd, struct stat* buf);

#endif
