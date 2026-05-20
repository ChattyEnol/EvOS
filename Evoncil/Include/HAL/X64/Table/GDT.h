/**HAL/X64/Table/GDT.h
 *
 * (C) Charity Enol
 *
 * 崭新的东西和知识！
 * 在 64 位模式下，CPU 强行关闭了大部分分段机制，
 * 把所有段的基地址（Base）都默认看作 0，内存空间变成了一片平坦。
 * 因此，现在的 GDT 已经不再用来划分内存块了，
 * 它退化成了纯粹的特权与架构宣告表。
 *
 * 它现在只负责两件事：
 *
 * 1. 宣告当前是 64 位长模式：
 * L 位（Long Mode 位）告诉 CPU，现在按 64 位指令集执行。
 *
 * 2. 界定特权级（DPL）：
 * 特权位控制当前是最高权限的 Ring 0（内核态）还是 Ring 3（用户态）。
 */

#ifndef HAL_X64_TABLE_GDT_H
#define HAL_X64_TABLE_GDT_H

#include <stdint.h>

/** Segment Descriptor
 *
 * 我去他奶奶的。
 * 这是比 IDT 更抽象的东西。
 * 定义在 Intel 手册的 3240 页，Figure 3-8。
 *
 *  31    24  23  22  21  20 19       16 15  14 13 12  11     8 7      0
 * +--------+---+---+---+---+-----------+---+-----+---+--------+--------+
 * |  Base  | G |D/B| L |AVL| Seg.Limit | P | DPL | S |  Type  |  Base  |
 * | 31..24 |   |   |   |   |   19..16  |   |     |   |        | 23..16 |
 * +--------+---+---+---+---+-----------+---+-----+---+--------+--------+
 *  31                                16 15                            0
 * +------------------------------------+-------------------------------+
 * |         Base Address 15..0         |      Segment Limit 15..0      |
 * +------------------------------------+-------------------------------+
 *
 * L     — 64-bit code segment (IA-32e mode only)
 * AVL   — Available for use by system software
 * BASE  — Segment base address
 * D/B   — Default operation size (0 = 16-bit segment; 1 = 32-bit segment)
 * DPL   — Descriptor privilege level
 * G     — Granularity
 * LIMIT — Segment Limit
 * P     — Segment present
 * S     — Descriptor type (0 = system; 1 = code or data)
 * TYPE  — Segment type
 */

// 预留硬编码的选择子，后面 IDT 填充 Selector 时直接用这些宏，绝对不会踩空！
#define GD_SELECTOR_KERNEL_CODE 0x08
#define GD_SELECTOR_KERNEL_DATA 0x10

/**
 * 64 位普通代码/数据段描述符（8 字节）。
 * 虽然 Base 和 Limit 在长模式下被 CPU 视而不见，
 * 但内存里必须老老实实摆成这个造型。
 */
typedef struct
{
    uint16_t Limit_15_0;            // 段界限低 16 位（全填 0 或 0xFFFF 即可）。
    uint16_t Base_15_0;             // 段基址低 16 位（全填 0）。
    uint8_t Base_23_16;             // 段基址中 8 位（全填 0）。
    uint8_t P_DPL_S_Type;           // 类型，比特数 1 + 2 + 1 + 4。
    uint8_t G_DB_L_ALV_Limit_19_16; // 比特数 1 + 1 + 1 + 1 + 4。
    uint8_t Base_31_24;             // 段基址高 8 位（全填 0）。
} __attribute__((packed)) GDT_DESCRIPTOR;

/**
 * 传递给 lgdt 指令的 GDTR 寄存器映像结构体。
 */
typedef struct
{
    uint16_t Limit;       // GDT 表的总字节数 - 1。
    uint64_t BaseAddress; // GDT 表在虚拟内存中的 64 位绝对起始地址。
} __attribute__((packed)) GDT_REGISTER;

/**
 * 初始化全局描述符表。
 * 设置属于内核自己的底层平坦段，并冲刷掉 UEFI 残留的旧 GDT。
 */
void InitGDT(void);

#endif