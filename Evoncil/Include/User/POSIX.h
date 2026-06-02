/** User/POSIX.h
 *
 * (C) Charity Enol
 *
 * 用户态 POSIX 风格系统调用封装。
 */

#ifndef USER_POSIX_H
#define USER_POSIX_H

#include <stdint.h>

#define ENOCALL_SYS_READ 0
#define ENOCALL_SYS_WRITE 1
#define ENOCALL_SYS_YIELD 24
#define ENOCALL_SYS_GETPID 39
#define ENOCALL_SYS_EXIT 60

/**
 * 通过当前平台的高性能入口请求内核服务。
 * X64 的这个函数定义在了中断服务例程的汇编里。
 * 别看你编辑器说找不到实现。
 */
int64_t Enocall(
    int64_t number,
    int64_t arg0,
    int64_t arg1,
    int64_t arg2,
    int64_t arg3,
    int64_t arg4,
    int64_t arg5);

int64_t read(int fd, void *buffer, uint64_t size);        // POSIX read 的极简封装。
int64_t write(int fd, const void *buffer, uint64_t size); // POSIX write 的极简封装。
void exit(int status);                                    // 结束当前进程。
int getpid(void);                                         // 获取当前进程 ID。
int sched_yield(void);                                    // 主动让出当前时间片。

#endif // USER_POSIX_H
