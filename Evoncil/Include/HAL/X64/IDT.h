/** Hal/X64/IDT.h
 *
 * (C) Charity Enol
 *
 * 这个中断处理的东西还是有点恐怖啊。
 * 详见 Intel 官方文档：
 * Intel® 64 and IA-32 Architectures Software Developer’s Manual.
 */

#ifndef HAL_X64_IDT_H
#define HAL_X64_IDT_H

#include <stdint.h>

/** 64-Bit IDT Gate Descriptors.
 *
 * 这绝对是写 OS 里最抽象的一个东西。
 * 定义在 Intel 官方手册 Intel® 64 and IA-32 Architectures Software Developer’s Manual。
 * 我的版本是 3364 页的 Figure 7-8。
 * 我的天，世界上还有这么厚的手册？
 *
 *  31                                                                    0
 * +-----------------------------------------------------------------------+
 * |                                Reserved                               |
 * +-----------------------------------------------------------------------+
 *
 *  31                                                                    0
 * +-----------------------------------------------------------------------+
 * |                             Offset 63..32                             |
 * +-----------------------------------------------------------------------+
 *
 *  31                               16 15  14 13 12  11   8 7      3 2   0
 * +-----------------------------------+---+-----+---+------+--------+-----+
 * |           Offset 31..16           | P | DPL | 0 | Type |  5'b0  | IST |
 * +-----------------------------------+---+-----+---+------+--------+-----+
 *
 *  31                               16 15                                0
 * +-----------------------------------+-----------------------------------+
 * |          Segment Selector         |           Offset 15..0            |
 * +-----------------------------------+-----------------------------------+
 *
 * DPL:      Descriptor Privilege Level (00 = Kernel, 11 = User)
 * Offset:   Offset to procedure entry point
 * P:        Segment Present (1 = Valid)
 * Selector: Segment Selector for destination code segment
 * IST:      Interrupt Stack Table Index (0 = Don't use IST)
 */
typedef struct
{
    uint16_t Offset_15_0;  // 偏移地址低 16 位。
    uint16_t Selector;     // 代码段选择器
    uint8_t IST;           // 中断栈表索引，只有 3bit，取值 0 ~ 7！
    uint8_t P_DPL_0_Type;  // 门类型和标志位，比特数分别为 1 + 2 + 1 + 4。
    uint16_t Offset_31_16; // 偏移地址中 16 位。
    uint32_t Offset_63_32; // 偏移地址高 32 位。
    uint32_t Reserved;     // 保留字段。
} __attribute__((packed)) IDT_GATE_DESCRIPTOR;

/**
 * AMD64 里有个专门的寄存器，叫做 IDTR。
 * 这个结构体里的东西就是这个寄存器的镜像。
 * 依旧，第一个成员变量是最低字节。
 */
typedef struct
{
    uint16_t Limit;       // IDT 大小 - 1（字节数）
    uint64_t BaseAddress; // IDT 基地址
} __attribute__((packed)) IDT_REGISTER;

// 写 AMD64 的 IDT 寄存器。
static inline void WriteIDTR(IDT_REGISTER *idtr);

#endif