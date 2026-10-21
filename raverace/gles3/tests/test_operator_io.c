/* Linker wrappers exist only in the host test, never in rr_gles3. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int inject(const char *operation, unsigned call)
{
    const char *trace = getenv("OPERATOR_TEST_TRACE");
    if (trace) {
        FILE *f = fopen(trace, "a");
        if (!f)
            abort();
        fprintf(f, "%s:%u\n", operation, call);
        if (fclose(f))
            abort();
    }
    char tag[64];
    snprintf(tag, sizeof tag, "%s:%u", operation, call);
    const char *requested = getenv("OPERATOR_TEST_FAIL");
    if (requested && !strcmp(tag, requested)) {
        errno = EIO;
        return 1;
    }
    return 0;
}

int __real_fsync(int fd);
int __wrap_fsync(int fd)
{
    static unsigned calls;
    return inject("fsync", ++calls) ? -1 : __real_fsync(fd);
}

ssize_t __real_write(int fd, const void *bytes, size_t size);
ssize_t __wrap_write(int fd, const void *bytes, size_t size)
{
    static unsigned calls;
    if (inject("write", ++calls))
        return -1;
    if (calls == 1 && getenv("OPERATOR_TEST_EINTR")) {
        errno = EINTR;
        return -1;
    }
    if (getenv("OPERATOR_TEST_SHORT_WRITE") && size > 7)
        size = 7;
    return __real_write(fd, bytes, size);
}

int __real_renameat(int olddir, const char *oldname, int newdir, const char *newname);
int __wrap_renameat(int olddir, const char *oldname, int newdir, const char *newname)
{
    static unsigned calls;
    return inject("rename", ++calls) ? -1 : __real_renameat(olddir, oldname, newdir, newname);
}

ssize_t __real_read(int fd, void *bytes, size_t size);
static unsigned read_calls;
static int read_inject(void)
{
    const char *path = getenv("OPERATOR_TEST_TRUNCATE_ON_READ");
    if (++read_calls == 1 && path) {
        int fd = open(path, O_WRONLY | O_CLOEXEC);
        if (fd < 0 || ftruncate(fd, 8191) || close(fd))
            abort();
    }
    return inject("read", read_calls);
}

ssize_t __wrap_read(int fd, void *bytes, size_t size)
{
    return read_inject() ? -1 : __real_read(fd, bytes, size);
}

/* The host's fortified libc lowers bounded read() calls to this symbol. */
ssize_t __real___read_chk(int fd, void *bytes, size_t size, size_t buffer_size);
ssize_t __wrap___read_chk(int fd, void *bytes, size_t size, size_t buffer_size)
{
    return read_inject() ? -1 : __real___read_chk(fd, bytes, size, buffer_size);
}
