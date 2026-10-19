#include "renderer_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
static void write_message(int fd, const char *line, size_t size)
{
    while (size) {
        ssize_t n = write(fd, line, size);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return;
        line += n;
        size -= (size_t)n;
    }
}
void gles3_unsupported(
    unsigned tick, unsigned main, unsigned sub, int code, int packet, const int *recipe, const char *why)
{
    char line[1024];
    int n = snprintf(line,
                     sizeof line,
                     "GLES3 UNSUPPORTED: tick=%u state=%u/%u key=%d/%d recipe=%d,%d,%d,%d,%d,%d,%d reason=%s\n",
                     tick,
                     main,
                     sub,
                     code,
                     packet,
                     recipe ? recipe[0] : -1,
                     recipe ? recipe[1] : -1,
                     recipe ? recipe[2] : -1,
                     recipe ? recipe[3] : -1,
                     recipe ? recipe[4] : -1,
                     recipe ? recipe[5] : -1,
                     recipe ? recipe[6] : -1,
                     why);
    if (n < 0)
        n = 0;
    if (n >= (int)sizeof line)
        n = sizeof line - 1;
    /* Direct writes survive even if another pipeline thread is blocked. The
     * supervisor also retains this exact first line when producing its summary. */
    write_message(STDERR_FILENO, line, n);
    const char *p = getenv("ENG_GLES3_SUMMARY_PATH");
    if (p) {
        int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            write_message(fd, line, n);
            close(fd);
        }
    }
    _Exit(86);
}
