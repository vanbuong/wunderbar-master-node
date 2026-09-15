/*
 * newlib-nano stubs so printf() is line-buffered on USB CDC.
 * SPDX-License-Identifier: MIT
 */

#include <sys/stat.h>

int _isatty(int fd)
{
    (void)fd;
    return 1;
}

int _close(int fd)
{
    (void)fd;
    return -1;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    if (st == 0) {
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

int _lseek(int fd, int ptr, int dir)
{
    (void)fd;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int fd, char *ptr, int len)
{
    (void)fd;
    (void)ptr;
    (void)len;
    return 0;
}
