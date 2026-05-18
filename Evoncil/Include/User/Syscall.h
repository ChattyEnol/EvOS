/** User/Syscall.h
 *
 * (C) Charity Enol
 *
 * 用户态 POSIX 风格系统调用封装。
 */

#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>

#define ENOL_SYS_READ 0
#define ENOL_SYS_WRITE 1
#define ENOL_SYS_YIELD 24
#define ENOL_SYS_GETPID 39
#define ENOL_SYS_EXIT 60

/**
 * 通过 INT 0x80 调用内核的 EnolCaller。
 */
int64_t EnolSyscall(
    int64_t number,
    int64_t arg0,
    int64_t arg1,
    int64_t arg2,
    int64_t arg3,
    int64_t arg4,
    int64_t arg5);

/**
 * POSIX read 的极简封装。
 */
int64_t read(int fd, void *buffer, uint64_t size);

/**
 * POSIX write 的极简封装。
 */
int64_t write(int fd, const void *buffer, uint64_t size);

/**
 * 结束当前进程。
 */
void exit(int status);

/**
 * 获取当前进程 ID。
 */
int getpid(void);

/**
 * 主动让出当前时间片。
 */
int sched_yield(void);

#endif // USER_SYSCALL_H
