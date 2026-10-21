/* Host-only preload guard: any personal file access is an immediate test failure. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void check(const char *path)
{
    const char *prefix = getenv("TITLE_POISON_DIR");
    if (prefix && path && !strncmp(path, prefix, strlen(prefix))) {
        const char message[] = "PERSONAL_FILE_OPENED\n";
        if (write(2, message, sizeof message - 1) < 0)
            _Exit(98);
        _Exit(99);
    }
}
FILE *fopen(const char *path, const char *mode)
{
    check(path);
    FILE *(*real)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen");
    return real(path, mode);
}
FILE *fopen64(const char *path, const char *mode)
{
    check(path);
    FILE *(*real)(const char *, const char *) = dlsym(RTLD_NEXT, "fopen64");
    return real(path, mode);
}
int open(const char *path, int flags, ...)
{
    check(path);
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list v;
        va_start(v, flags);
        mode = va_arg(v, int);
        va_end(v);
    }
    int (*real)(const char *, int, ...) = dlsym(RTLD_NEXT, "open");
    return real(path, flags, mode);
}
int openat(int dir, const char *path, int flags, ...)
{
    check(path);
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list v;
        va_start(v, flags);
        mode = va_arg(v, int);
        va_end(v);
    }
    int (*real)(int, const char *, int, ...) = dlsym(RTLD_NEXT, "openat");
    return real(dir, path, flags, mode);
}
