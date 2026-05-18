/** User/UserCall.c
 *
 * (C) Charity Enol
 *
 * INT 0x80 用户态系统调用封装。
 */

#include <User/Syscall.h>

#include <stdint.h>

int64_t EnolSyscall(
    int64_t number,
    int64_t arg0,
    int64_t arg1,
    int64_t arg2,
    int64_t arg3,
    int64_t arg4,
    int64_t arg5)
{
    register int64_t rax __asm__("rax") = number;
    register int64_t rdi __asm__("rdi") = arg0;
    register int64_t rsi __asm__("rsi") = arg1;
    register int64_t rdx __asm__("rdx") = arg2;
    register int64_t r10 __asm__("r10") = arg3;
    register int64_t r8 __asm__("r8") = arg4;
    register int64_t r9 __asm__("r9") = arg5;

    __asm__ volatile(
        "int $0x80"
        : "+a"(rax)
        : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory");

    return rax;
}

int64_t read(int fd, void *buffer, uint64_t size)
{
    return EnolSyscall(ENOL_SYS_READ, fd, (int64_t)(uintptr_t)buffer, (int64_t)size, 0, 0, 0);
}

int64_t write(int fd, const void *buffer, uint64_t size)
{
    return EnolSyscall(ENOL_SYS_WRITE, fd, (int64_t)(uintptr_t)buffer, (int64_t)size, 0, 0, 0);
}

void exit(int status)
{
    (void)EnolSyscall(ENOL_SYS_EXIT, status, 0, 0, 0, 0, 0);

    while (1)
        (void)sched_yield();
}

int getpid(void)
{
    return (int)EnolSyscall(ENOL_SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

int sched_yield(void)
{
    return (int)EnolSyscall(ENOL_SYS_YIELD, 0, 0, 0, 0, 0, 0);
}
