/** HAL/HAL.h
 * 
 * (C) 2026 Charity Enol
 * 
 * 强迫症真的很难受！
 * 这个头文件里声明的函数我想达到一个“平台无关的硬件抽象”。
 * 详细来说，也就是提供硬件的“基础业务名称”，而不出现平台的寄存器名这些。
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

// 停顿。
void Halt(void);

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

// 开启全局中断标志位（IF）。
void EnableInterrupts(void);
// 关闭全局中断标志位（IF）。
void DisableInterrupts(void);

// 初始化平台的整个中断控制系统。
void InitInterrupt(WORLD *world);
void InitAPIC(void *acpi_root);
const char *GetInterruptControllerName(void);
uint32_t GetLocalAPICID(void);
uint32_t GetMSIMessageAddress(void);
uint32_t GetMSIMessageData(uint8_t vector);

/**
 * 将特定的处理程序绑定到特定的中断向量号上。
 * @param vector 中断向量号。
 * @param handler 汇编跳板或处理函数的地址。
 */
void SetInterruptGate(uint8_t vector, void *handler);
void SetHardwareInterrupt(uint8_t irq, uint8_t vector);
void SendEndOfInterrupt(void);

// 中断处理函数原型。
typedef void (*HandleInterrupt)(void);

/** 
 * 寄存器和 MSR 操作。
 */

// 用于 CPU 特定配置、APIC 访问等。

uint64_t ReadMSR(uint32_t msr_id);
void WriteMSR(uint32_t msr_id, uint64_t value);
void ReadCPUID(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx);
uint64_t ReadControlRegister(uint32_t reg_num);
void WriteControlRegister(uint32_t reg_num, uint64_t value);

// 设备端口读。
uint8_t ReadHardwarePortByte(uint16_t port);
// 设备端口写。
void WriteHardwarePortByte(uint16_t port, uint8_t value);

#endif // HAL_HAL_H
