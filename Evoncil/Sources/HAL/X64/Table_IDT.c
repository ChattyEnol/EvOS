/** HAL/X64/Table/IDT.c
 *
 * (C) Charity Enol
 *
 * Interrupt 里的东西太多了，出来分担一点。
 */

#include <HAL/X64/Table/IDT.h>
#include <HAL/X64/Interrupt.h>

#include <stddef.h>

static IDT_DESCRIPTOR IDT_TABLE[256]; // 静态分配内存，内核逻辑看不见它。

/**
 * 设置 IDT 里某一个中断的入口。
 * @param vector 中断号。
 * @param isr_stub 中断服务例程跳板。
 * @param dpl 权限，用户是 3，内核是 0，见手册或者注释。
 */
static void SetIDTEntry(uint8_t vector, void *isr_stub, uint8_t dpl);

// 写 IDT 寄存器。
static inline void WriteIDTR(IDT_REGISTER *idtr) { __asm__ volatile("lidt (%0)" ::"r"(idtr)); }

// 声明汇编里的那些跳板函数。
#define INTERRUPT_OPT(vector) extern void ISRStub##vector(void);
INTERRUPT_VECTORS
#undef INTERRUPT_OPT

// 初始化 IDT 表。
#define INTERRUPT_OPT(vector) SetIDTEntry(vector, ISRStub##vector, vector == 128 ? 3 : 0);
static void InitIDT(void)
{
    // 先把 IDT 表里所有项都清空，免得它们里有垃圾数据。
    for (uint32_t index = 0; index < 256; index++)
        SetIDTEntry((uint8_t)index, NULL, 0);
    INTERRUPT_VECTORS
    IDT_REGISTER idtr;
    idtr.Limit = (sizeof(IDT_DESCRIPTOR) * 256) - 1;
    idtr.BaseAddress = (uint64_t)&IDT_TABLE;
    WriteIDTR(&idtr);
}
#undef INTERRUPT_OPT

static void SetIDTEntry(uint8_t vector, void *isr_stub, uint8_t dpl)
{
    // 空跳板先全用 `0` 占位。
    if (isr_stub == NULL)
    {
        IDT_TABLE[vector].Offset_15_0 = 0;
        IDT_TABLE[vector].Selector = 0;
        IDT_TABLE[vector].IST = 0;
        IDT_TABLE[vector].P_DPL_0_Type = 0;
        IDT_TABLE[vector].Offset_31_16 = 0;
        IDT_TABLE[vector].Offset_63_32 = 0;
        IDT_TABLE[vector].Reserved = 0;
        return;
    }
    // 由于 IDT 项的地址是整数类型，因此要转换数据类型。
    // 但也可以运行时转换，减少一丁点内存开销。
    IDT_TABLE[vector].Offset_15_0 = ((uint64_t)isr_stub) & 0xFFFF;
    IDT_TABLE[vector].Selector = 0x08;
    IDT_TABLE[vector].IST = 0;
    IDT_TABLE[vector].P_DPL_0_Type = (uint8_t)(((dpl & 0x3u) << 5) | 0x8E);
    IDT_TABLE[vector].Offset_31_16 = (((uint64_t)isr_stub) >> 16) & 0xFFFF;
    IDT_TABLE[vector].Offset_63_32 = (((uint64_t)isr_stub) >> 32) & 0xFFFFFFFF;
    IDT_TABLE[vector].Reserved = 0;
}
