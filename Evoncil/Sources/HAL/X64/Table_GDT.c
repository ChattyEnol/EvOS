/** HAL/X64/Table_GDT.c
 *
 * (C) Charity Enol
 *
 * 纯手工打造的内核钢铁城堡。
 */

#include <HAL/X64/Table/GDT.h>
#include <stddef.h>

// 静态分配 3 个表项，对外完全隐藏
static GDT_DESCRIPTOR GDT_TABLE[3];

void InitGDT(void)
{
    // 第 0 项：Null 描述符，硬件强行要求全清零
    GDT_TABLE[0].Limit_15_0 = 0;
    GDT_TABLE[0].Base_15_0 = 0;
    GDT_TABLE[0].Base_23_16 = 0;
    GDT_TABLE[0].P_DPL_S_Type = 0;
    GDT_TABLE[0].G_DB_L_ALV_Limit_19_16 = 0;
    GDT_TABLE[0].Base_31_24 = 0;

    // 第 1 项：内核代码段 (Selector: 0x08)
    // Type=0xA (可执行/可读代码), S=1 (代码/数据), DPL=00 (Ring 0), P=1 -> 10011010b = 0x9A
    // G=1 (4K粒度), D=0 (64位下代码段此位必为0), L=1 (64位长模式), AVL=0 -> 10100000b = 0xA0
    GDT_TABLE[1].Limit_15_0 = 0;
    GDT_TABLE[1].Base_15_0 = 0;
    GDT_TABLE[1].Base_23_16 = 0;
    GDT_TABLE[1].P_DPL_S_Type = 0x9A;
    GDT_TABLE[1].G_DB_L_ALV_Limit_19_16 = 0xA0;
    GDT_TABLE[1].Base_31_24 = 0;

    // 第 2 项：内核数据段 (Selector: 0x10)
    // Type=0x2 (可读写数据), S=1 (代码/数据), DPL=00 (Ring 0), P=1 -> 10010010b = 0x92
    // G=1 (4K粒度), D=1 (32位/64位兼容数据属性), L=0 (数据段此位必为0), AVL=0 -> 11000000b = 0xC0
    GDT_TABLE[2].Limit_15_0 = 0;
    GDT_TABLE[2].Base_15_0 = 0;
    GDT_TABLE[2].Base_23_16 = 0;
    GDT_TABLE[2].P_DPL_S_Type = 0x92;
    GDT_TABLE[2].G_DB_L_ALV_Limit_19_16 = 0xC0;
    GDT_TABLE[2].Base_31_24 = 0;

    // 准备好交给 lgdt 的寄存器映像
    GDT_REGISTER gdtr;
    gdtr.Limit = sizeof(GDT_TABLE) - 1;
    gdtr.BaseAddress = (uint64_t)&GDT_TABLE;

    // 激情澎湃的内联汇编：加载 GDT 并强制刷新所有段寄存器
    __asm__ volatile(
        "lgdt %0\n\t"               // 把我们新盖的城堡地址告诉 CPU
        "mov $0x10, %%ax\n\t"       // 0x10 是内核数据段选择子
        "mov %%ax, %%ds\n\t"        // 刷新数据段
        "mov %%ax, %%es\n\t"        // 刷新附加段
        "mov %%ax, %%ss\n\t"        // 刷新核心栈段
        "mov %%ax, %%fs\n\t"        // 顺手刷了 fs
        "mov %%ax, %%gs\n\t"        // 顺手刷了 gs
        "pushq $0x08\n\t"           // 把新的内核代码段选择子 0x08 压栈
        "leaq 1f(%%rip), %%rax\n\t" // 把标签 1 的绝对虚拟地址载入 rax
        "pushq %%rax\n\t"           // 压栈，伪造好远返回的现场
        "lretq\n\t"                 // 经典的远返回指令，迫使 CPU 弹出并刷新 CS！
        "1:\n\t"                    // 从这一刻起，CPU 真正跑在属于我们自己的平坦内核段之上了！
        :
        : "m"(gdtr)
        : "rax", "memory");
}