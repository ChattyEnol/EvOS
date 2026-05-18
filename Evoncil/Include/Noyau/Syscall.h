/** Noyau/Syscall.h
 *
 * (C) Charity Enol
 *
 * 内核系统调用入口。
 */

#ifndef NOYAU_SYSCALL_H
#define NOYAU_SYSCALL_H

#include <HAL/HAL.h>
#include <stdint.h>

typedef enum
{
    ENOL_SYS_READ = 0,
    ENOL_SYS_WRITE = 1,
    ENOL_SYS_YIELD = 24,
    ENOL_SYS_GETPID = 39,
    ENOL_SYS_EXIT = 60
} ENOL_SYSCALL_NUMBER;

/**
 * EvOS 的系统调用分发器。
 * INT 0x80 的汇编入口会把寄存器现场交给这里。
 */
uint64_t EnolCaller(INTERRUPT_FRAME *frame);

#endif // NOYAU_SYSCALL_H
