/** HAL/X64/Interrupt.h
 *
 * (C) Charity Enol
 *
 * X64 中断控制硬件。
 * APIC / MSI，X64 的高级中断控制器。
 */

#ifndef HAL_X64_INTERRUPT_H
#define HAL_X64_INTERRUPT_H

#include <HAL/HAL.h>
#include <stdint.h>

/**
 * 把所有要启用的中断号用宏清单列出来。
 * 同时有一个统一的操作宏 `INTERRUPT_OPT`，
 * 使用时可以用现场定义操作宏。
 * 操作完要去定义！
 */
#define INTERRUPT_VECTORS \
    INTERRUPT_OPT(0)      \
    INTERRUPT_OPT(1)      \
    INTERRUPT_OPT(2)      \
    INTERRUPT_OPT(3)      \
    INTERRUPT_OPT(4)      \
    INTERRUPT_OPT(5)      \
    INTERRUPT_OPT(6)      \
    INTERRUPT_OPT(7)      \
    INTERRUPT_OPT(8)      \
    INTERRUPT_OPT(9)      \
    INTERRUPT_OPT(10)     \
    INTERRUPT_OPT(11)     \
    INTERRUPT_OPT(12)     \
    INTERRUPT_OPT(13)     \
    INTERRUPT_OPT(14)     \
    INTERRUPT_OPT(16)     \
    INTERRUPT_OPT(17)     \
    INTERRUPT_OPT(18)     \
    INTERRUPT_OPT(19)     \
    INTERRUPT_OPT(20)     \
    INTERRUPT_OPT(21)     \
    INTERRUPT_OPT(32)     \
    INTERRUPT_OPT(80)     \
    INTERRUPT_OPT(128)    \
    INTERRUPT_OPT(255)

// 汇编中断入口保存下来的通用寄存器和硬件栈帧。
// EnolCaller 会直接修改这里的 RAX，把系统调用返回值带回去。
typedef struct
{
    uint64_t R15;
    uint64_t R14;
    uint64_t R13;
    uint64_t R12;
    uint64_t R11;
    uint64_t R10;
    uint64_t R9;
    uint64_t R8;
    uint64_t RDI;
    uint64_t RSI;
    uint64_t RBP;
    uint64_t RBX;
    uint64_t RDX;
    uint64_t RCX;
    uint64_t RAX;
    uint64_t Vector;
    uint64_t ErrorCode;
    uint64_t RIP;
    uint64_t CS;
    uint64_t RFLAGS;
    uint64_t RSP;
    uint64_t SS;
} INTERRUPT_FRAME;

typedef void (*HandleInterrupt)(void); // 中断处理函数原型。

void InitInterrupt(void *acpi_root); // 初始化平台的整个中断控制系统。

#endif // HAL_X64_INTERRUPT_H
