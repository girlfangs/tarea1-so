#include "io.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

static int create_fmt(char **strp, const char *fmt, va_list args) {
    return vasprintf(strp, fmt, args);
}

static int write_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while(len > 0) {
        ssize_t written = write(fd, p, len);
        if(written < 0) {
            if(errno == EINTR) continue;
            return -1;
        }
        p += written;
        len -= written;
    }
    return 0;
}

int fout(const char *fmt, ...) {
    va_list args;
    char *strp;
    int ret = 0;

    va_start(args, fmt);

    create_fmt(&strp, fmt, args);

    ret = write_all(STDOUT_FILENO, strp, strlen(strp));

    va_end(args);
    free(strp);

    return ret;
}

int ferr(const char *fmt, ...) {
    va_list args;
    char *strp;
    int ret = 0;

    va_start(args, fmt);

    create_fmt(&strp, fmt, args);

    ret = write_all(STDERR_FILENO, strp, strlen(strp));

    va_end(args);
    free(strp);

    return ret;
}
