/** HAL/X64/Registers.h
 *
 * (C) Charity Enol
 *
 * X64 特殊寄存器读写。
 */

#ifndef HAL_X64_REGISTERS_H
#define HAL_X64_REGISTERS_H

#include <HAL/HAL.h>
#include <stdint.h>

/**
 * 模型特定寄存器 MSR（Model Specific Registers）。
 * 这是 Intel / AMD 为了实现一些现代高级特性而专门开辟的通道。
 * 比如高性能系统调用指令 syscall / sysret，
 * 就必须提前把内核入口地址写进 MSR 寄存器里，
 * CPU 才能实现不查中断门就瞬间切入 Ring 0。
 */

uint64_t ReadMSR(uint32_t msr_id);
void WriteMSR(uint32_t msr_id, uint64_t value);

/**
 * 控制寄存器（CR0..CR4），CPU 最高总开关。
 * CR0 决定要不要开启内存分页。
 * CR1 是 USART 总控制寄存器，决定串口行为（似乎不重要）。
 * CR2 则专门用来存放上一次引发缺页异常（Page Fault）的那个倒霉虚拟地址。
 * CR3 存放着当前页表的根地址。
 */

uint64_t ReadControlRegister(uint32_t reg_num);
void WriteControlRegister(uint32_t reg_num, uint64_t value);

#endif // HAL_X64_REGISTERS_H
