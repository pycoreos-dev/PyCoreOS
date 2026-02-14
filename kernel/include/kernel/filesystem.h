#ifndef KERNEL_FILESYSTEM_H
#define KERNEL_FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum fs_backend {
    FS_BACKEND_RAM = 0,
    FS_BACKEND_BOOT_MODULE = 1,
} fs_backend;

void fs_init(void);
void fs_reset_ramdisk(void);
bool fs_import_module(const char* name, const void* data, size_t size);
size_t fs_count(void);
bool fs_name_at(size_t index, char* out, size_t out_cap);
bool fs_backend_at(size_t index, fs_backend* out_backend);
bool fs_size_at(size_t index, size_t* out_size);
bool fs_read(const char* name, char* out, size_t out_cap);
bool fs_read_bytes(const char* name, size_t offset, void* out, size_t out_cap, size_t* out_read);
bool fs_map_readonly(const char* name, const uint8_t** out_data, size_t* out_size);
bool fs_write(const char* name, const char* content);
bool fs_write_bytes(const char* name, const void* data, size_t size);
bool fs_touch(const char* name);
bool fs_remove(const char* name);
bool fs_exists(const char* name);
bool fs_size(const char* name, size_t* out_size);
size_t fs_ramdisk_used(void);
size_t fs_ramdisk_capacity(void);
size_t fs_serialize_ramdisk(uint8_t* out, size_t out_cap);
bool fs_deserialize_ramdisk(const uint8_t* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
