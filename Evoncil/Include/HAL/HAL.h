/** HAL/HAL.h
 *
 * (C) Charity Enol
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
 * 平台无关的系统调用现场。
 * HAL 负责把不同架构的寄存器约定翻译成这份结构。
 */
typedef struct
{
    uint64_t Number;       // 系统调用号。
    uint64_t Arguments[6]; // 最多 6 个通用参数。
    uint64_t Result;       // 返回给调用方的结果。
} SYSTEM_CALL_CONTEXT;

typedef uint64_t (*HandleSystemCall)(SYSTEM_CALL_CONTEXT *context); // 系统调用处理函数原型。

/**
 * 处理器控制。
 */

void InitHardware(WORLD *); // 全局初始化。
void Halt(void);            // 停顿。
void Pause(void);           // 暂停（让出 CPU 时间片）。

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

void EnableInterrupts(void);                             // 开中断。
void DisableInterrupts(void);                            // 关中断。
void SetInterruptHandler(uint8_t vector, void *handler); // 绑定某个中断号的处理程序。
void SetHardwareVector(uint8_t irq, uint8_t vector);     // 将某个硬件中断号绑定到某个中断向量上。
void InitSystemCall(HandleSystemCall handler);           // 初始化高性能系统调用入口。

/**
 * Hypervisor 平台相关的函数。
 */

void InitHypervisor(uint64_t message_address, uint64_t event_address); // 配置虚拟化通信内存。
uint64_t Hypercall(uint64_t code, uint64_t param);                     // 发起虚拟机呼叫。
// 告诉宿主机当前消息已处理完毕，请清理槽位并重置通知线。
void Hypereceive(void);

#endif // HAL_HAL_H
