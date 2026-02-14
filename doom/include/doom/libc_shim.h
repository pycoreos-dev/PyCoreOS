/*
 * libc_shim.h — Freestanding C library shim for DOOM on PyCoreOS
 *
 * Provides declarations for standard C library functions that DOOM
 * expects but do not exist in a -ffreestanding environment.
 */
#ifndef DOOM_LIBC_SHIM_H
#define DOOM_LIBC_SHIM_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Memory allocation ---- */
void* malloc(size_t size);
void  free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t nmemb, size_t size);
#define alloca __builtin_alloca

/* ---- String/memory operations ---- */
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int   memcmp(const void* s1, const void* s2, size_t n);

size_t strlen(const char* s);
char*  strcpy(char* dest, const char* src);
char*  strncpy(char* dest, const char* src, size_t n);
int    strcmp(const char* s1, const char* s2);
int    strncmp(const char* s1, const char* s2, size_t n);
int    strcasecmp(const char* s1, const char* s2);
int    strncasecmp(const char* s1, const char* s2, size_t n);
char*  strcat(char* dest, const char* src);
char*  strncat(char* dest, const char* src, size_t n);
char*  strchr(const char* s, int c);
char*  strrchr(const char* s, int c);
char*  strstr(const char* haystack, const char* needle);
char*  strdup(const char* s);

/* ---- Formatted I/O ---- */
int printf(const char* fmt, ...);
int fprintf(void* stream, const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
int snprintf(char* buf, size_t size, const char* fmt, ...);
int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap);
int vfprintf(void* stream, const char* fmt, va_list ap);
int sscanf(const char* str, const char* fmt, ...);
int puts(const char* s);
int putchar(int c);
int fputc(int c, void* stream);
int fputs(const char* s, void* stream);
void setbuf(void* stream, char* buf);
int getchar(void);

/* ---- File I/O stubs ---- */
typedef struct _doom_FILE { int dummy; } FILE;
#ifndef EOF
#define EOF (-1)
#endif

/* We provide extern pointers for stdin/stdout/stderr */
extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE*  fopen(const char* path, const char* mode);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int    fclose(FILE* stream);
int    fseek(FILE* stream, long offset, int whence);
long   ftell(FILE* stream);
int    fflush(FILE* stream);
int    feof(FILE* stream);
int    fgetc(FILE* stream);
char*  fgets(char* s, int size, FILE* stream);

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

int    remove(const char* path);
int    rename(const char* oldpath, const char* newpath);

/* ---- POSIX file I/O stubs ---- */
#ifndef O_RDONLY
#define O_RDONLY  0
#define O_WRONLY  1
#define O_RDWR    2
#define O_CREAT   0x40
#define O_TRUNC   0x200
#define O_BINARY  0
#endif

int   open(const char* path, int flags, ...);
int   read(int fd, void* buf, size_t count);
int   close(int fd);
long  lseek(int fd, long offset, int whence);
int   write(int fd, const void* buf, size_t count);
int   access(const char* path, int mode);

#ifndef F_OK
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

/* ---- Conversion ---- */
int   atoi(const char* nptr);
long  atol(const char* nptr);
int   abs(int j);

/* ---- Character classification ---- */
int toupper(int c);
int tolower(int c);
int isdigit(int c);
int isspace(int c);
int isalpha(int c);
int isprint(int c);
int isupper(int c);
int islower(int c);
int isalnum(int c);

/* ---- Process ---- */
void exit(int status);
char* getenv(const char* name);

/* ---- Error ---- */
extern int errno;
char* strerror(int errnum);

/* ---- Sort ---- */
void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*));

/* ---- Math (minimal, integer-only for DOOM) ---- */
/* DOOM uses fixed-point, no floating-point math needed */

/* ---- Misc POSIX stubs ---- */
typedef int pid_t;
typedef unsigned int mode_t;
typedef long off_t;
typedef long ssize_t;
typedef long time_t;

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval* tv, struct timezone* tz);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);

/* ---- Signals (stubs) ---- */
typedef void (*sighandler_t)(int);
#define SIG_ERR ((sighandler_t)-1)
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIGINT  2
#define SIGTERM 15
sighandler_t signal(int signum, sighandler_t handler);

/* ---- mkdir stub ---- */
int mkdir(const char* path, unsigned int mode);

#ifdef __cplusplus
}
#endif

#endif /* DOOM_LIBC_SHIM_H */
