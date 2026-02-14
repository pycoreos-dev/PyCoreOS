#ifndef KERNEL_FS_PERSIST_H
#define KERNEL_FS_PERSIST_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void fs_persist_init(void);
bool fs_persist_available(void);
bool fs_persist_save_now(void);
bool fs_persist_load_now(void);
bool fs_save_to_disk(void);
bool fs_load_from_disk(void);

#ifdef __cplusplus
}
#endif

#endif
