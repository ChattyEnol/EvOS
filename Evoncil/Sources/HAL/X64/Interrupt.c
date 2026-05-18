/** HAL/X64/Interrupt.c
 *
 * (C) Charity Enol
 *
 * AMD64 IDT 和中断分发。
 */

#include <HAL/HAL.h>
#include <HAL/X64/IDT.h>

#include <stddef.h>

// 静态分配内存，内核逻辑看不见它。
static IDT_GATE_DESCRIPTOR IDT_TABLE[256];

// 把所有要启用的中断号用宏清单列出来。
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
    INTERRUPT_OPT(33)     \
    INTERRUPT_OPT(34)     \
    INTERRUPT_OPT(255)

// 声明汇编里的那些跳板函数。
#define INTERRUPT_OPT(vector) extern void ISRStub##vector(void);
INTERRUPT_VECTORS
#undef INTERRUPT_OPT

// 准备一个数组，存放业务层的 C 处理函数。
static HandleInterrupt InterruptHandlers[256] = {0};

static void SetIDTEntry(uint8_t vector, void *isr_stub);
static void InitIDT(void);
static inline void WriteIDTR(IDT_REGISTER *idtr);

void InitInterrupt(WORLD *world)
{
    DisableInterrupts();
    InitIDT();

    if (world != NULL)
        InitAPIC(world->AcpiRoot);
    else
        InitAPIC(NULL);
}

// 通用分发中心（这个函数会被 ISR.s 里的汇编调用）。
void CommonInterruptHandler(uint64_t vector, uint64_t error_code)
{
    (void)error_code;

    if (vector >= 32)
    {
        if (InterruptHandlers[vector] != NULL)
            InterruptHandlers[vector]();

        if (vector != 255)
            SendEndOfInterrupt();

        return;
    }

    if (InterruptHandlers[vector] != NULL)
    {
        InterruptHandlers[vector]();
        return;
    }

    Halt();
}

// 开放给上层业务用的注册接口。
void SetInterruptGate(uint8_t vector, void *handler)
{
    InterruptHandlers[vector] = (HandleInterrupt)handler;
}

static void InitIDT(void)
{
    for (uint32_t index = 0; index < 256; index++)
        SetIDTEntry((uint8_t)index, NULL);

#define INTERRUPT_OPT(vector) SetIDTEntry(vector, ISRStub##vector);
    INTERRUPT_VECTORS
#undef INTERRUPT_OPT

    IDT_REGISTER idtr;
    idtr.Limit = (sizeof(IDT_GATE_DESCRIPTOR) * 256) - 1;
    idtr.BaseAddress = (uint64_t)&IDT_TABLE;
    WriteIDTR(&idtr);
}

static void SetIDTEntry(uint8_t vector, void *isr_stub)
{
    uint64_t address = (uint64_t)isr_stub;
    IDT_TABLE[vector].Offset_15_0 = address & 0xFFFF;
    IDT_TABLE[vector].Selector = 0x08;
    IDT_TABLE[vector].IST = 0;
    IDT_TABLE[vector].P_DPL_0_Type = 0x8E;
    IDT_TABLE[vector].Offset_31_16 = (address >> 16) & 0xFFFF;
    IDT_TABLE[vector].Offset_63_32 = (address >> 32) & 0xFFFFFFFF;
    IDT_TABLE[vector].Reserved = 0;
}

static inline void WriteIDTR(IDT_REGISTER *idtr)
{
    __asm__ volatile("lidt (%0)" ::"r"(idtr));
}
