#include "kernel/filesystem.h"

#include <stddef.h>
#include <stdint.h>

enum {
    kRamMaxFiles = 64,
    kRamNameMax = 48,
    kRamDataMax = 4096,
    kModuleMaxFiles = 8,
    kModuleNameMax = 64,
};

typedef struct ram_file {
    bool used;
    char name[kRamNameMax];
    size_t size;
    uint8_t data[kRamDataMax];
} ram_file;

typedef struct module_file {
    bool used;
    char name[kModuleNameMax];
    const uint8_t* data;
    size_t size;
} module_file;

static ram_file s_ram_files[kRamMaxFiles];
static module_file s_module_files[kModuleMaxFiles];

static size_t cstr_len(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static bool str_eq(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        ++i;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static bool copy_cstr(char* dst, size_t cap, const char* src) {
    if (dst == NULL || src == NULL || cap == 0) {
        return false;
    }
    size_t i = 0;
    while (src[i] != '\0' && i + 1 < cap) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    return src[i] == '\0';
}

static size_t min_size(size_t a, size_t b) {
    return (a < b) ? a : b;
}

static int find_ram_file(const char* name) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    for (size_t i = 0; i < kRamMaxFiles; ++i) {
        if (s_ram_files[i].used && str_eq(s_ram_files[i].name, name)) {
            return (int)i;
        }
    }
    return -1;
}

static int find_module_file(const char* name) {
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    for (size_t i = 0; i < kModuleMaxFiles; ++i) {
        if (s_module_files[i].used && str_eq(s_module_files[i].name, name)) {
            return (int)i;
        }
    }
    return -1;
}

static int alloc_ram_slot(void) {
    for (size_t i = 0; i < kRamMaxFiles; ++i) {
        if (!s_ram_files[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static int alloc_module_slot(void) {
    for (size_t i = 0; i < kModuleMaxFiles; ++i) {
        if (!s_module_files[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static void reset_modules(void) {
    for (size_t i = 0; i < kModuleMaxFiles; ++i) {
        s_module_files[i].used = false;
        s_module_files[i].name[0] = '\0';
        s_module_files[i].data = NULL;
        s_module_files[i].size = 0;
    }
}

void fs_reset_ramdisk(void) {
    for (size_t i = 0; i < kRamMaxFiles; ++i) {
        s_ram_files[i].used = false;
        s_ram_files[i].name[0] = '\0';
        s_ram_files[i].size = 0;
    }
}

void fs_init(void) {
    fs_reset_ramdisk();
    reset_modules();

    (void)fs_write("readme.txt", "Welcome to PyCoreOS virtual filesystem.");
    (void)fs_write("notes.txt", "Try: help, apps, open calc, find, head, tail, grep, wc, todo add, journal add");
    (void)fs_write("settings.cfg", "mouse_speed=2\ntheme=0\nresolution_mode=0\n");
}

bool fs_import_module(const char* name, const void* data, size_t size) {
    if (name == NULL || name[0] == '\0' || data == NULL || size == 0) {
        return false;
    }
    if (find_ram_file(name) >= 0) {
        return false;
    }

    int existing = find_module_file(name);
    if (existing >= 0) {
        s_module_files[existing].data = (const uint8_t*)data;
        s_module_files[existing].size = size;
        return true;
    }

    int slot = alloc_module_slot();
    if (slot < 0) {
        return false;
    }

    module_file* f = &s_module_files[slot];
    f->used = true;
    if (!copy_cstr(f->name, sizeof(f->name), name)) {
        f->used = false;
        f->name[0] = '\0';
        f->data = NULL;
        f->size = 0;
        return false;
    }
    f->data = (const uint8_t*)data;
    f->size = size;
    return true;
}

size_t fs_count(void) {
    size_t total = 0;
    for (size_t i = 0; i < kRamMaxFiles; ++i) {
        if (s_ram_files[i].used) {
            ++total;
        }
    }
    for (size_t i = 0; i < kModuleMaxFiles; ++i) {
        if (s_module_files[i].used) {
            ++total;
        }
    }
    return total;
}

static bool file_at(size_t index, fs_backend* out_backend, size_t* out_slot) {
    size_t n = 0;

    for (size_t i = 0; i < kRamMaxFiles; ++i) {
        if (!s_ram_files[i].used) {
            continue;
        }
        if (n == index) {
            if (out_backend != NULL) {
                *out_backend = FS_BACKEND_RAM;
            }
            if (out_slot != NULL) {
                *out_slot = i;
            }
            return true;
        }
        ++n;
    }

    for (size_t i = 0; i < kModuleMaxFiles; ++i) {
        if (!s_module_files[i].used) {
            continue;
        }
        if (n == index) {
            if (out_backend != NULL) {
                *out_backend = FS_BACKEND_BOOT_MODULE;
            }
            if (out_slot != NULL) {
                *out_slot = i;
            }
            return true;
        }
        ++n;
    }

    return false;
}

bool fs_name_at(size_t index, char* out, size_t out_cap) {
    fs_backend backend;
    size_t slot = 0;
    if (!file_at(index, &backend, &slot)) {
        return false;
    }

    if (backend == FS_BACKEND_RAM) {
        return copy_cstr(out, out_cap, s_ram_files[slot].name);
    }
    return copy_cstr(out, out_cap, s_module_files[slot].name);
}

bool fs_backend_at(size_t index, fs_backend* out_backend) {
    if (out_backend == NULL) {
        return false;
    }
    size_t slot = 0;
    return file_at(index, out_backend, &slot);
}

bool fs_size_at(size_t index, size_t* out_size) {
    if (out_size == NULL) {
        return false;
    }

    fs_backend backend;
    size_t slot = 0;
    if (!file_at(index, &backend, &slot)) {
        return false;
    }

    if (backend == FS_BACKEND_RAM) {
        *out_size = s_ram_files[slot].size;
        return true;
    }

    *out_size = s_module_files[slot].size;
    return true;
}

bool fs_exists(const char* name) {
    return find_ram_file(name) >= 0 || find_module_file(name) >= 0;
}

bool fs_size(const char* name, size_t* out_size) {
    if (out_size == NULL || name == NULL || name[0] == '\0') {
        return false;
    }

    const int ram_idx = find_ram_file(name);
    if (ram_idx >= 0) {
        *out_size = s_ram_files[ram_idx].size;
        return true;
    }

    const int mod_idx = find_module_file(name);
    if (mod_idx >= 0) {
        *out_size = s_module_files[mod_idx].size;
        return true;
    }

    return false;
}

bool fs_read_bytes(const char* name, size_t offset, void* out, size_t out_cap, size_t* out_read) {
    if (out_read != NULL) {
        *out_read = 0;
    }
    if (name == NULL || out == NULL || out_cap == 0) {
        return false;
    }

    const int ram_idx = find_ram_file(name);
    if (ram_idx >= 0) {
        const ram_file* f = &s_ram_files[ram_idx];
        if (offset >= f->size) {
            return true;
        }
        const size_t bytes = min_size(out_cap, f->size - offset);
        uint8_t* dst = (uint8_t*)out;
        for (size_t i = 0; i < bytes; ++i) {
            dst[i] = f->data[offset + i];
        }
        if (out_read != NULL) {
            *out_read = bytes;
        }
        return true;
    }

    const int mod_idx = find_module_file(name);
    if (mod_idx >= 0) {
        const module_file* f = &s_module_files[mod_idx];
        if (offset >= f->size) {
            return true;
        }
        const size_t bytes = min_size(out_cap, f->size - offset);
        uint8_t* dst = (uint8_t*)out;
        for (size_t i = 0; i < bytes; ++i) {
            dst[i] = f->data[offset + i];
        }
        if (out_read != NULL) {
            *out_read = bytes;
        }
        return true;
    }

    return false;
}

bool fs_map_readonly(const char* name, const uint8_t** out_data, size_t* out_size) {
    if (name == NULL || out_data == NULL || out_size == NULL) {
        return false;
    }

    const int ram_idx = find_ram_file(name);
    if (ram_idx >= 0) {
        *out_data = s_ram_files[ram_idx].data;
        *out_size = s_ram_files[ram_idx].size;
        return true;
    }

    const int mod_idx = find_module_file(name);
    if (mod_idx >= 0) {
        *out_data = s_module_files[mod_idx].data;
        *out_size = s_module_files[mod_idx].size;
        return true;
    }

    return false;
}

bool fs_read(const char* name, char* out, size_t out_cap) {
    if (name == NULL || out == NULL || out_cap == 0) {
        return false;
    }

    size_t read_bytes = 0;
    if (!fs_read_bytes(name, 0, out, out_cap - 1, &read_bytes)) {
        return false;
    }

    for (size_t i = 0; i < read_bytes; ++i) {
        const uint8_t b = (uint8_t)out[i];
        const bool printable = (b >= 32U && b <= 126U) || b == '\n' || b == '\r' || b == '\t';
        if (!printable) {
            out[i] = '.';
        }
    }
    out[read_bytes] = '\0';
    return true;
}

bool fs_write_bytes(const char* name, const void* data, size_t size) {
    if (name == NULL || data == NULL || name[0] == '\0') {
        return false;
    }
    if (size > kRamDataMax) {
        return false;
    }
    if (find_module_file(name) >= 0) {
        return false;
    }

    int idx = find_ram_file(name);
    if (idx < 0) {
        idx = alloc_ram_slot();
        if (idx < 0) {
            return false;
        }
        s_ram_files[idx].used = true;
        if (!copy_cstr(s_ram_files[idx].name, sizeof(s_ram_files[idx].name), name)) {
            s_ram_files[idx].used = false;
            s_ram_files[idx].name[0] = '\0';
            s_ram_files[idx].size = 0;
            return false;
        }
    }

    ram_file* f = &s_ram_files[idx];
    f->size = size;
    const uint8_t* src = (const uint8_t*)data;
    for (size_t i = 0; i < size; ++i) {
        f->data[i] = src[i];
    }
    return true;
}

bool fs_write(const char* name, const char* content) {
    if (name == NULL || content == NULL) {
        return false;
    }
    const size_t len = cstr_len(content);
    return fs_write_bytes(name, content, len);
}

bool fs_touch(const char* name) {
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    if (find_module_file(name) >= 0) {
        return false;
    }

    int idx = find_ram_file(name);
    if (idx >= 0) {
        return true;
    }

    idx = alloc_ram_slot();
    if (idx < 0) {
        return false;
    }

    s_ram_files[idx].used = true;
    if (!copy_cstr(s_ram_files[idx].name, sizeof(s_ram_files[idx].name), name)) {
        s_ram_files[idx].used = false;
        s_ram_files[idx].name[0] = '\0';
        return false;
    }
    s_ram_files[idx].size = 0;
    return true;
}

bool fs_remove(const char* name) {
    const int idx = find_ram_file(name);
    if (idx < 0) {
        return false;
    }

    s_ram_files[idx].used = false;
    s_ram_files[idx].name[0] = '\0';
    s_ram_files[idx].size = 0;
    return true;
}

size_t fs_ramdisk_used(void) {
    size_t total = 0;
    for (size_t i = 0; i < kRamMaxFiles; ++i) {
        if (s_ram_files[i].used) {
            total += s_ram_files[i].size;
        }
    }
    return total;
}

size_t fs_ramdisk_capacity(void) {
    return (size_t)kRamMaxFiles * (size_t)kRamDataMax;
}

static bool append_bytes(uint8_t* out, size_t out_cap, size_t* cursor, const void* src, size_t len) {
    if (cursor == NULL || src == NULL) {
        return false;
    }
    if (*cursor + len > out_cap) {
        return false;
    }

    uint8_t* dst = out + *cursor;
    const uint8_t* bytes = (const uint8_t*)src;
    for (size_t i = 0; i < len; ++i) {
        dst[i] = bytes[i];
    }
    *cursor += len;
    return true;
}

static bool append_u32(uint8_t* out, size_t out_cap, size_t* cursor, uint32_t value) {
    uint8_t raw[4];
    raw[0] = (uint8_t)(value & 0xFFU);
    raw[1] = (uint8_t)((value >> 8U) & 0xFFU);
    raw[2] = (uint8_t)((value >> 16U) & 0xFFU);
    raw[3] = (uint8_t)((value >> 24U) & 0xFFU);
    return append_bytes(out, out_cap, cursor, raw, sizeof(raw));
}

static bool read_u32(const uint8_t* data, size_t size, size_t* cursor, uint32_t* out) {
    if (data == NULL || cursor == NULL || out == NULL) {
        return false;
    }
    if (*cursor + 4 > size) {
        return false;
    }

    const uint8_t* p = data + *cursor;
    *out = (uint32_t)p[0] |
           ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) |
           ((uint32_t)p[3] << 24U);
    *cursor += 4;
    return true;
}

size_t fs_serialize_ramdisk(uint8_t* out, size_t out_cap) {
    size_t cursor = 0;
    const char magic[4] = {'P', 'Y', 'F', 'S'};

    if (!append_bytes(out, out_cap, &cursor, magic, sizeof(magic))) {
        return 0;
    }

    uint32_t count = 0;
    for (size_t i = 0; i < kRamMaxFiles; ++i) {
        if (s_ram_files[i].used) {
            ++count;
        }
    }

    if (!append_u32(out, out_cap, &cursor, 1U)) {
        return 0;
    }
    if (!append_u32(out, out_cap, &cursor, count)) {
        return 0;
    }

    for (size_t i = 0; i < kRamMaxFiles; ++i) {
        if (!s_ram_files[i].used) {
            continue;
        }

        const ram_file* f = &s_ram_files[i];
        const size_t name_len = cstr_len(f->name);
        if (name_len == 0 || name_len > 255U) {
            return 0;
        }

        const uint8_t name_len_u8 = (uint8_t)name_len;
        if (!append_bytes(out, out_cap, &cursor, &name_len_u8, sizeof(name_len_u8))) {
            return 0;
        }
        if (!append_u32(out, out_cap, &cursor, (uint32_t)f->size)) {
            return 0;
        }
        if (!append_bytes(out, out_cap, &cursor, f->name, name_len)) {
            return 0;
        }
        if (!append_bytes(out, out_cap, &cursor, f->data, f->size)) {
            return 0;
        }
    }

    return cursor;
}

bool fs_deserialize_ramdisk(const uint8_t* data, size_t size) {
    if (data == NULL || size < 12) {
        return false;
    }

    if (data[0] != 'P' || data[1] != 'Y' || data[2] != 'F' || data[3] != 'S') {
        return false;
    }

    size_t cursor = 4;
    uint32_t version = 0;
    uint32_t count = 0;
    if (!read_u32(data, size, &cursor, &version) || version != 1U) {
        return false;
    }
    if (!read_u32(data, size, &cursor, &count)) {
        return false;
    }

    fs_reset_ramdisk();

    for (uint32_t i = 0; i < count; ++i) {
        if (cursor + 1 > size) {
            fs_init();
            return false;
        }

        const uint8_t name_len = data[cursor++];
        uint32_t file_size = 0;
        if (name_len == 0 || !read_u32(data, size, &cursor, &file_size)) {
            fs_init();
            return false;
        }
        if (file_size > kRamDataMax) {
            fs_init();
            return false;
        }
        if (cursor + (size_t)name_len + (size_t)file_size > size) {
            fs_init();
            return false;
        }

        char name[kRamNameMax];
        if ((size_t)name_len >= sizeof(name)) {
            fs_init();
            return false;
        }
        for (uint8_t j = 0; j < name_len; ++j) {
            name[j] = (char)data[cursor + j];
        }
        name[name_len] = '\0';
        cursor += (size_t)name_len;

        if (!fs_write_bytes(name, data + cursor, (size_t)file_size)) {
            fs_init();
            return false;
        }
        cursor += (size_t)file_size;
    }

    return true;
}
