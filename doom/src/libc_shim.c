/*
 * libc_shim.c — Freestanding C library implementation for DOOM on PyCoreOS
 *
 * Provides minimal implementations of standard C library functions
 * needed by the DOOM engine in a freestanding environment.
 */
#include "doom/libc_shim.h"
#include "kernel/serial.h"

/* ======================================================================
 * HEAP — simple bump allocator from a static array
 * ====================================================================== */

#define HEAP_SIZE (8 * 1024 * 1024) /* 8 MB */

static char s_heap[HEAP_SIZE] __attribute__((aligned(16)));
static size_t s_heap_offset = 0;

/* Simple block header for free-list awareness (minimal) */
typedef struct alloc_hdr {
    size_t size;
    uint32_t magic;
} alloc_hdr_t;

#define ALLOC_MAGIC 0xA110CA7E

static size_t align_up(size_t v, size_t a) {
    return (v + a - 1) & ~(a - 1);
}

void* malloc(size_t size) {
    if (size == 0) size = 1;
    size_t total = align_up(sizeof(alloc_hdr_t) + size, 16);
    if (s_heap_offset + total > HEAP_SIZE) {
        serial_write("[DOOM] malloc: out of memory!\n");
        return (void*)0;
    }
    alloc_hdr_t* hdr = (alloc_hdr_t*)&s_heap[s_heap_offset];
    hdr->size = total;
    hdr->magic = ALLOC_MAGIC;
    s_heap_offset += total;
    return (void*)(hdr + 1);
}

void free(void* ptr) {
    /* Bump allocator: free is a no-op. Memory is never reclaimed. */
    (void)ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return (void*)0; }
    alloc_hdr_t* hdr = ((alloc_hdr_t*)ptr) - 1;
    size_t old_size = 0;
    if (hdr->magic == ALLOC_MAGIC) {
        old_size = hdr->size - sizeof(alloc_hdr_t);
    }
    void* newp = malloc(size);
    if (newp && old_size > 0) {
        size_t copy = old_size < size ? old_size : size;
        memcpy(newp, ptr, copy);
    }
    return newp;
}

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

/* ======================================================================
 * String/memory operations
 * ====================================================================== */

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* a = (const unsigned char*)s1;
    const unsigned char* b = (const unsigned char*)s2;
    while (n--) {
        if (*a != *b) return *a - *b;
        a++; b++;
    }
    return 0;
}

size_t strlen(const char* s) {
    const char* p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int _to_lower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && _to_lower(*s1) == _to_lower(*s2)) { s1++; s2++; }
    return _to_lower(*(unsigned char*)s1) - _to_lower(*(unsigned char*)s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && _to_lower(*s1) == _to_lower(*s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return _to_lower(*(unsigned char*)s1) - _to_lower(*(unsigned char*)s2);
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (*d) d++;
    while (n-- && (*d = *src++)) d++;
    *d = '\0';
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : (void*)0;
}

char* strrchr(const char* s, int c) {
    const char* last = (void*)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char*)s;
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0)
            return (char*)haystack;
        haystack++;
    }
    return (void*)0;
}

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* p = (char*)malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

/* ======================================================================
 * Formatted I/O — minimal vsnprintf implementation
 * ====================================================================== */

/* Internal helper: write an integer to buffer */
static int _int_to_str(char* buf, size_t cap, int value, int base, int is_signed, int width, int pad_zero, int precision) {
    char tmp[32];
    int neg = 0;
    unsigned int uval;
    int len = 0;
    int i;

    if (is_signed && value < 0) {
        neg = 1;
        uval = (unsigned int)(-(value + 1)) + 1u;
    } else {
        uval = (unsigned int)value;
    }

    if (uval == 0) {
        if (precision != 0) {
            tmp[len++] = '0';
        }
    } else {
        while (uval > 0) {
            unsigned int d = uval % (unsigned int)base;
            tmp[len++] = (d < 10) ? ('0' + (char)d) : ('a' + (char)(d - 10));
            uval /= (unsigned int)base;
        }
    }

    int zero_pad = 0;
    if (precision > len) {
        zero_pad = precision - len;
    }

    int total = len + zero_pad + neg;
    int pad = (width > total) ? (width - total) : 0;
    int written = 0;
    int use_zero_pad_char = (pad_zero && precision < 0);

    if (use_zero_pad_char && neg && written < (int)cap) {
        buf[written++] = '-';
        neg = 0;
    }
    for (i = 0; i < pad && written < (int)cap; i++) {
        buf[written++] = use_zero_pad_char ? '0' : ' ';
    }
    if (neg && written < (int)cap) {
        buf[written++] = '-';
    }
    for (i = 0; i < zero_pad && written < (int)cap; i++) {
        buf[written++] = '0';
    }
    for (i = len - 1; i >= 0 && written < (int)cap; i--) {
        buf[written++] = tmp[i];
    }
    return written;
}

static int _uint_to_hex(char* buf, size_t cap, unsigned int value, int width, int pad_zero, int uppercase, int precision) {
    char tmp[16];
    int len = 0;
    int i;

    if (value == 0) {
        if (precision != 0) {
            tmp[len++] = '0';
        }
    } else {
        while (value > 0) {
            unsigned int d = value & 0xF;
            if (d < 10) tmp[len++] = '0' + (char)d;
            else tmp[len++] = (uppercase ? 'A' : 'a') + (char)(d - 10);
            value >>= 4;
        }
    }

    int zero_pad = 0;
    if (precision > len) {
        zero_pad = precision - len;
    }

    int total = len + zero_pad;
    int pad = (width > total) ? (width - total) : 0;
    int written = 0;
    int use_zero_pad_char = (pad_zero && precision < 0);
    for (i = 0; i < pad && written < (int)cap; i++) {
        buf[written++] = use_zero_pad_char ? '0' : ' ';
    }
    for (i = 0; i < zero_pad && written < (int)cap; i++) {
        buf[written++] = '0';
    }
    for (i = len - 1; i >= 0 && written < (int)cap; i--) {
        buf[written++] = tmp[i];
    }
    return written;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
    size_t pos = 0;
    if (size == 0) return 0;
    size_t cap = size - 1;

    while (*fmt) {
        if (*fmt != '%') {
            if (pos < cap) buf[pos] = *fmt;
            pos++;
            fmt++;
            continue;
        }
        fmt++; /* skip '%' */

        /* Parse flags */
        int pad_zero = 0;
        int left_align = 0;
        while (*fmt == '0' || *fmt == '-') {
            if (*fmt == '0') pad_zero = 1;
            if (*fmt == '-') left_align = 1;
            fmt++;
        }
        (void)left_align;

        /* Parse width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        int precision = -1;
        /* Parse precision */
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                precision = precision * 10 + (*fmt - '0');
                fmt++;
            }
        }

        /* Length modifier */
        int is_long = 0;
        if (*fmt == 'l') { is_long = 1; fmt++; if (*fmt == 'l') fmt++; }
        if (*fmt == 'h') { fmt++; if (*fmt == 'h') fmt++; }

        switch (*fmt) {
        case 'd': case 'i': {
            int val = is_long ? (int)va_arg(ap, long) : va_arg(ap, int);
            int n = _int_to_str(buf + (pos < cap ? pos : cap), cap > pos ? cap - pos : 0, val, 10, 1, width, pad_zero, precision);
            pos += (size_t)n;
            break;
        }
        case 'u': {
            unsigned int val = is_long ? (unsigned int)va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            int n = _int_to_str(buf + (pos < cap ? pos : cap), cap > pos ? cap - pos : 0, (int)val, 10, 0, width, pad_zero, precision);
            pos += (size_t)n;
            break;
        }
        case 'x': {
            unsigned int val = is_long ? (unsigned int)va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            int n = _uint_to_hex(buf + (pos < cap ? pos : cap), cap > pos ? cap - pos : 0, val, width, pad_zero, 0, precision);
            pos += (size_t)n;
            break;
        }
        case 'X': {
            unsigned int val = is_long ? (unsigned int)va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            int n = _uint_to_hex(buf + (pos < cap ? pos : cap), cap > pos ? cap - pos : 0, val, width, pad_zero, 1, precision);
            pos += (size_t)n;
            break;
        }
        case 'p': {
            unsigned int val = (unsigned int)(uintptr_t)va_arg(ap, void*);
            if (pos < cap) buf[pos] = '0';
            pos++;
            if (pos < cap) buf[pos] = 'x';
            pos++;
            int n = _uint_to_hex(buf + (pos < cap ? pos : cap), cap > pos ? cap - pos : 0, val, 8, 1, 0, -1);
            pos += (size_t)n;
            break;
        }
        case 's': {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s) {
                if (pos < cap) buf[pos] = *s;
                pos++; s++;
            }
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (pos < cap) buf[pos] = c;
            pos++;
            break;
        }
        case '%':
            if (pos < cap) buf[pos] = '%';
            pos++;
            break;
        case 'f': case 'g': case 'e': {
            /* DOOM doesn't really use float printf, but absorb the arg */
            (void)va_arg(ap, double);
            const char* fp = "<float>";
            while (*fp) { if (pos < cap) buf[pos] = *fp; pos++; fp++; }
            break;
        }
        default:
            if (pos < cap) buf[pos] = '%';
            pos++;
            if (pos < cap) buf[pos] = *fmt;
            pos++;
            break;
        }
        if (*fmt) fmt++;
    }

    buf[pos < cap ? pos : cap] = '\0';
    return (int)pos;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, 4096, fmt, ap); /* assume buffer is large enough */
    va_end(ap);
    return r;
}

int vfprintf(void* stream, const char* fmt, va_list ap) {
    char tmp[512];
    int r = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    serial_write(tmp);
    (void)stream;
    return r;
}

int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf((void*)0, fmt, ap);
    va_end(ap);
    return r;
}

int fprintf(void* stream, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}

int sscanf(const char* str, const char* fmt, ...) {
    /* Minimal sscanf: only supports %d and %s for what DOOM needs */
    va_list ap;
    va_start(ap, fmt);
    int count = 0;

    while (*fmt && *str) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd' || *fmt == 'i') {
                int* p = va_arg(ap, int*);
                int val = 0, neg = 0;
                while (*str == ' ') str++;
                if (*str == '-') { neg = 1; str++; }
                if (*str < '0' || *str > '9') break;
                while (*str >= '0' && *str <= '9') {
                    val = val * 10 + (*str - '0');
                    str++;
                }
                *p = neg ? -val : val;
                count++;
                fmt++;
            } else if (*fmt == 's') {
                char* p = va_arg(ap, char*);
                while (*str && *str != ' ' && *str != '\n')
                    *p++ = *str++;
                *p = '\0';
                count++;
                fmt++;
            } else {
                fmt++;
            }
        } else {
            if (*fmt == *str) { fmt++; str++; }
            else break;
        }
    }

    va_end(ap);
    return count;
}

int puts(const char* s) {
    serial_write(s);
    serial_write("\n");
    return 0;
}

int putchar(int c) {
    char buf[2] = {(char)c, '\0'};
    serial_write(buf);
    return c;
}

int fputc(int c, void* stream) {
    (void)stream;
    return putchar(c);
}

int fputs(const char* s, void* stream) {
    (void)stream;
    serial_write(s);
    return 0;
}

/* ======================================================================
 * File I/O stubs
 * ====================================================================== */

static FILE _stdin_obj, _stdout_obj, _stderr_obj;
FILE* stdin = &_stdin_obj;
FILE* stdout = &_stdout_obj;
FILE* stderr = &_stderr_obj;

FILE* fopen(const char* path, const char* mode) {
    (void)path; (void)mode;
    return (FILE*)0;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    (void)ptr; (void)size; (void)nmemb; (void)stream;
    return 0;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
    /* If writing to stdout/stderr, output via serial */
    if (stream == stdout || stream == stderr) {
        const char* p = (const char*)ptr;
        size_t total = size * nmemb;
        /* Quick & dirty: just output character by character */
        for (size_t i = 0; i < total; i++) {
            char buf[2] = {p[i], '\0'};
            serial_write(buf);
        }
        return nmemb;
    }
    return 0;
}

int fclose(FILE* stream) { (void)stream; return 0; }
int fseek(FILE* stream, long offset, int whence) { (void)stream; (void)offset; (void)whence; return -1; }
long ftell(FILE* stream) { (void)stream; return -1; }
int fflush(FILE* stream) { (void)stream; return 0; }
int feof(FILE* stream) { (void)stream; return 1; }
int fgetc(FILE* stream) { (void)stream; return EOF; }
char* fgets(char* s, int size, FILE* stream) { (void)s; (void)size; (void)stream; return (char*)0; }
void setbuf(void *stream, char *buf) { (void)stream; (void)buf; }
int getchar(void) { return EOF; }

int remove(const char* path) { (void)path; return -1; }
int rename(const char* o, const char* n) { (void)o; (void)n; return -1; }
int mkdir(const char* path, unsigned int mode) { (void)path; (void)mode; return -1; }

/* ======================================================================
 * POSIX file I/O stubs — DOOM's w_wad.c uses open/read/lseek/close
 * We redirect these to the in-memory WAD loaded via multiboot module
 * ====================================================================== */

#include "kernel/filesystem.h"

/* We support up to 4 simultaneously open "files" */
#define MAX_OPEN_FDS 4

typedef struct {
    int in_use;
    const uint8_t* data;
    size_t size;
    size_t pos;
} memfd_t;

static memfd_t s_fds[MAX_OPEN_FDS];

static bool fs_find_name_case_insensitive(const char* name, char* out, size_t out_cap) {
    if (name == (void*)0 || out == (void*)0 || out_cap == 0) return false;

    const size_t total = fs_count();
    char candidate[64];
    for (size_t i = 0; i < total; i++) {
        if (!fs_name_at(i, candidate, sizeof(candidate))) {
            continue;
        }
        if (strcasecmp(candidate, name) == 0) {
            strncpy(out, candidate, out_cap - 1);
            out[out_cap - 1] = '\0';
            return true;
        }
    }

    return false;
}

static bool fs_map_readonly_anycase(const char* name, const uint8_t** out_data, size_t* out_size) {
    if (fs_map_readonly(name, out_data, out_size)) {
        return true;
    }

    char resolved[64];
    if (fs_find_name_case_insensitive(name, resolved, sizeof(resolved))) {
        return fs_map_readonly(resolved, out_data, out_size);
    }

    return false;
}

static bool fs_exists_anycase(const char* name) {
    if (fs_exists(name)) {
        return true;
    }

    char resolved[64];
    return fs_find_name_case_insensitive(name, resolved, sizeof(resolved));
}

int open(const char* path, int flags, ...) {
    (void)flags;
    /* Find the basename */
    const char* base = path;
    const char* p = path;
    while (*p) {
        if (*p == '/' || *p == '\\') base = p + 1;
        p++;
    }

    /* Try to map the file from our filesystem */
    const uint8_t* data = 0;
    size_t size = 0;
    if (!fs_map_readonly_anycase(base, &data, &size)) {
        /* Also try the full path */
        if (!fs_map_readonly_anycase(path, &data, &size)) {
            return -1;
        }
    }

    /* Find a free fd slot (start from 3 to avoid stdin/stdout/stderr) */
    for (int i = 0; i < MAX_OPEN_FDS; i++) {
        if (!s_fds[i].in_use) {
            s_fds[i].in_use = 1;
            s_fds[i].data = data;
            s_fds[i].size = size;
            s_fds[i].pos = 0;
            return i + 3; /* fd 3, 4, 5, 6 */
        }
    }
    return -1;
}

int read(int fd, void* buf, size_t count) {
    int idx = fd - 3;
    if (idx < 0 || idx >= MAX_OPEN_FDS || !s_fds[idx].in_use) return -1;
    memfd_t* f = &s_fds[idx];
    if (f->pos >= f->size) return 0;
    size_t avail = f->size - f->pos;
    if (count > avail) count = avail;
    memcpy(buf, f->data + f->pos, count);
    f->pos += count;
    return (int)count;
}

int close(int fd) {
    int idx = fd - 3;
    if (idx < 0 || idx >= MAX_OPEN_FDS) return -1;
    s_fds[idx].in_use = 0;
    return 0;
}

long lseek(int fd, long offset, int whence) {
    int idx = fd - 3;
    if (idx < 0 || idx >= MAX_OPEN_FDS || !s_fds[idx].in_use) return -1;
    memfd_t* f = &s_fds[idx];
    long new_pos;
    switch (whence) {
        case 0: new_pos = offset; break; /* SEEK_SET */
        case 1: new_pos = (long)f->pos + offset; break; /* SEEK_CUR */
        case 2: new_pos = (long)f->size + offset; break; /* SEEK_END */
        default: return -1;
    }
    if (new_pos < 0) new_pos = 0;
    if ((size_t)new_pos > f->size) new_pos = (long)f->size;
    f->pos = (size_t)new_pos;
    return new_pos;
}

int write(int fd, const void* buf, size_t count) {
    /* Redirect fd 1 (stdout) and 2 (stderr) to serial */
    if (fd == 1 || fd == 2) {
        const char* p = (const char*)buf;
        for (size_t i = 0; i < count; i++) {
            char tmp[2] = {p[i], '\0'};
            serial_write(tmp);
        }
        return (int)count;
    }
    (void)buf;
    return -1;
}

int access(const char* path, int mode) {
    (void)mode;
    /* Check basename in filesystem */
    const char* base = path;
    const char* p = path;
    while (*p) { if (*p == '/' || *p == '\\') base = p + 1; p++; }
    if (fs_exists_anycase(base)) return 0;
    if (fs_exists_anycase(path)) return 0;
    return -1;
}

/* fstat — get file size from our in-memory fd table */
/* Include the struct stat definition */
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

int fstat(int fd, struct stat* buf) {
    int idx = fd - 3;
    if (idx < 0 || idx >= MAX_OPEN_FDS || !s_fds[idx].in_use) return -1;
    memset(buf, 0, sizeof(struct stat));
    buf->st_size = (unsigned long)s_fds[idx].size;
    return 0;
}

/* fscanf — minimal implementation for DOOM's config file parsing */
int fscanf(void* stream, const char* fmt, ...) {
    (void)stream; (void)fmt;
    /* Config file loading uses fscanf — since we can't read files,
       just return 0 (no items matched) */
    return 0;
}

/* ======================================================================
 * Conversion
 * ====================================================================== */

int atoi(const char* nptr) {
    int val = 0, neg = 0;
    while (*nptr == ' ' || *nptr == '\t') nptr++;
    if (*nptr == '-') { neg = 1; nptr++; }
    else if (*nptr == '+') nptr++;
    while (*nptr >= '0' && *nptr <= '9') {
        val = val * 10 + (*nptr - '0');
        nptr++;
    }
    return neg ? -val : val;
}

long atol(const char* nptr) { return (long)atoi(nptr); }

int abs(int j) { return j < 0 ? -j : j; }

/* ======================================================================
 * Character classification
 * ====================================================================== */

int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isprint(int c) { return c >= 0x20 && c < 0x7f; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isalnum(int c) { return isalpha(c) || isdigit(c); }

/* ======================================================================
 * Process
 * ====================================================================== */

extern int doom_should_quit;

void exit(int status) {
    (void)status;
    serial_write("[DOOM] exit() called\n");
    doom_should_quit = 1;
    return;
}

char* getenv(const char* name) {
    if (name == (void*)0) return (char*)0;

    if (strcmp(name, "DOOMWADDIR") == 0) {
        return (char*)".";
    }
    if (strcmp(name, "HOME") == 0) {
        return (char*)".";
    }

    return (char*)0;
}

/* ======================================================================
 * Error
 * ====================================================================== */

int errno = 0;

char* strerror(int errnum) {
    (void)errnum;
    return (char*)"error";
}

/* ======================================================================
 * Sort — simple insertion sort (DOOM uses qsort in a few places)
 * ====================================================================== */

void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*)) {
    char* arr = (char*)base;
    /* Small temp buffer on stack for swap */
    char tmp[256];
    if (size > sizeof(tmp)) return; /* safety */

    for (size_t i = 1; i < nmemb; i++) {
        memcpy(tmp, arr + i * size, size);
        size_t j = i;
        while (j > 0 && compar(arr + (j - 1) * size, tmp) > 0) {
            memcpy(arr + j * size, arr + (j - 1) * size, size);
            j--;
        }
        memcpy(arr + j * size, tmp, size);
    }
}

/* ======================================================================
 * POSIX time stubs
 * ====================================================================== */

/* 64-bit division helper for 32-bit systems (needed for gettimeofday) */
unsigned long long __udivdi3(unsigned long long n, unsigned long long d) {
    unsigned long long q = 0;
    if (d == 0) return 0; /* division by zero */
    
    /* Simple software division (slow but correct) */
    /* Only handles common case where d is small (s_tsc_per_us is ~3000) */
    while (n >= d) {
        n -= d;
        q++;
    }
    return q;
}

unsigned long long __udivmoddi4(unsigned long long n, unsigned long long d, unsigned long long *r) {
    unsigned long long q = 0;
    if (d == 0) return 0;
    
    while (n >= d) {
        n -= d;
        q++;
    }
    if (r) *r = n;
    return q;
}

/* We use rdtsc for timekeeping — same as timing.c */
static inline uint64_t _rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t s_boot_tsc = 0;
static uint32_t s_tsc_per_us = 3000; /* approx 3 GHz, will be close enough */

int gettimeofday(struct timeval* tv, struct timezone* tz) {
    if (s_boot_tsc == 0) s_boot_tsc = _rdtsc();
    uint64_t elapsed = _rdtsc() - s_boot_tsc;
    uint64_t us = elapsed / s_tsc_per_us; 
    if (tv) {
        tv->tv_sec = (long)(us / 1000000ULL);
        tv->tv_usec = (long)(us % 1000000ULL);
    }
    return 0;
}
