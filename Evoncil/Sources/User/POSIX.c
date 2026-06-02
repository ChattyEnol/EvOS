/** User/POSIX.c
 *
 * (C) Charity Enol
 *
 * 用户态 POSIX 风格系统调用封装。
 */

#include <User/POSIX.h>

#include <stdint.h>

int64_t read(int fd, void *buffer, uint64_t size)
{
    return Enocall(
        ENOCALL_SYS_READ, fd,
        (int64_t)(uintptr_t)buffer, (int64_t)size,
        0, 0, 0);
}

int64_t write(int fd, const void *buffer, uint64_t size)
{
    return Enocall(
        ENOCALL_SYS_WRITE, fd,
        (int64_t)(uintptr_t)buffer, (int64_t)size,
        0, 0, 0);
}

void exit(int status)
{
    (void)Enocall(ENOCALL_SYS_EXIT, status, 0, 0, 0, 0, 0);

    while (1)
        (void)sched_yield();
}

int getpid(void) { return (int)Enocall(ENOCALL_SYS_GETPID, 0, 0, 0, 0, 0, 0); }

int sched_yield(void) { return (int)Enocall(ENOCALL_SYS_YIELD, 0, 0, 0, 0, 0, 0); }
