/** HAL/HAL.h
 *
 * (C) 2026 Charity Enol
 *
 * 强迫症真的很难受！
 * 这个头文件里声明的函数我想达到一个“平台无关的硬件抽象”。
 * 详细来说，也就是提供硬件的“基础业务名称”，而不出现平台相关的硬件名和操作。
 * 相当于，要对 `X64/` 以及以后可能拓展的 `ARM64/` 下的直接硬件抽象再做一次封装。
 * 虽然麻烦，但似乎真的很清爽啊！
 */

#ifndef HAL_HAL_H
#define HAL_HAL_H

#include <World/World.h>
#include <stdint.h>

/**
 * 处理器控制。
 */

void InitHardware(WORLD *); // 全局初始化。
void Halt(void);            // 停顿。

/**
 * 内存管理部分的硬件操作封装。
 * 基本上都是页表管理。
 */

uint64_t GetPageTableRoot(void);
void SetPageTableRoot(uint64_t root);
void InvalidateTLB(void *virtual_address);
void FlushTLB(void);

/**
 * 中断相关。
 * 平台各异，因此要参考 `HAL/<PLATFORM>/` 下的东西。
 */

void EnableInterrupts(void);                          // 开启全局中断标志位（IF）。
void DisableInterrupts(void);                         // 关闭全局中断标志位（IF）。
void SetInterruptGate(uint8_t vector, void *handler); // 将特定的处理程序绑定到特定的中断向量号上。
void SetHardwareInterrupt(uint8_t irq, uint8_t vector);
void SendEndOfInterrupt(void);

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

#endif // HAL_HAL_H
