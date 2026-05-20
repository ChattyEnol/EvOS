/** HAL/X64/Interrupt.c
 *
 * (C) Charity Enol
 *
 * X64 中断分发。
 */

#include <HAL/HAL.h>
#include <HAL/X64/Interrupt.h>
#include <HAL/X64/Table/IDT.h>
#include <Noyau/Syscall.h>

#include <UI/TextIO.h>

#include <stddef.h>

// 静态分配内存，内核逻辑看不见它。
static IDT_DESCRIPTOR IDT_TABLE[256];

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
    INTERRUPT_OPT(33)     \
    INTERRUPT_OPT(34)     \
    INTERRUPT_OPT(128)    \
    INTERRUPT_OPT(255)

// 声明汇编里的那些跳板函数。
#define INTERRUPT_OPT(vector) extern void ISRStub##vector(void);
INTERRUPT_VECTORS
#undef INTERRUPT_OPT

// 准备一个数组，存放业务层的 C 处理函数。
static HandleInterrupt InterruptHandlers[256] = {0};

/**
 * 初始化 IDT 表。
 * 先全部清零，
 * 然后把 IDTR（寄存器）指向这个表的基址。
 */
static void InitIDT(void);

/**
 * 设置 IDT 里某一个中断的入口。
 * @param vector 中断号。
 * @param isr_stub 中断服务例程跳板。
 * @param dpl 权限，用户是 3，内核是 0，见手册或者注释。
 */
static void SetIDTEntry(uint8_t vector, void *isr_stub, uint8_t dpl);

// 写 IDT 寄存器。
static inline void WriteIDTR(IDT_REGISTER *idtr);

/**
 * 下面开始实现 `HAL/HAL.h` 内的统一接口。
 */

void EnableInterrupts(void) { __asm__ volatile("sti"); }
void DisableInterrupts(void) { __asm__ volatile("cli"); }

void InitInterrupt(void *acpi_root)
{
    DisableInterrupts();
    InitIDT();
    InitAPIC(acpi_root);
}

// 通用分发中心（这个函数会被 `ISR.s` 里的汇编调用）。
void CommonInterruptHandler(uint64_t vector, uint64_t error_code, INTERRUPT_FRAME *frame)
{
    (void)error_code;

    // 率先拦截所有异常。
    if (vector > 32)
    {
        kprintf("\n!!! KERNEL PANIC: CPU EXCEPTION %d !!!\n", (int)vector);
        kprintf("RIP: %d   CS:  %d   RFLAGS: %d\n", frame->RIP, frame->CS, frame->RFLAGS);
        kprintf("RSP: %d   SS:  %d   ERROR:  %d\n", frame->RSP, frame->SS, frame->ErrorCode);
        kprintf("RAX: %d   RBX: %d   RCX:    %d\n", frame->RAX, frame->RBX, frame->RCX);
        kprintf("RDI: %d   RSI: %d   RBP:    %d\n", frame->RDI, frame->RSI, frame->RBP);
        kprintf("R8:  %d   R9:  %d   R10:    %d\n", frame->R8, frame->R9, frame->R10);

        // 如果是经典的 14 号缺页异常，必须把 CR2 读出来看看它到底在访问哪里的野指针。
        if (vector == 14)
        {
            uint64_t cr2_val;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2_val));
            kprintf("CR2 (Fault Address): 0x%016llX\n", cr2_val);
        }

        while (1)
            Halt();
    }

    if (vector == 128)
    {
        EnolCaller(frame);
        return;
    }

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

    while (1)
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
        SetIDTEntry((uint8_t)index, NULL, 0);

#define INTERRUPT_OPT(vector) SetIDTEntry(vector, ISRStub##vector, vector == 128 ? 3 : 0);
    INTERRUPT_VECTORS
#undef INTERRUPT_OPT

    IDT_REGISTER idtr;
    idtr.Limit = (sizeof(IDT_DESCRIPTOR) * 256) - 1;
    idtr.BaseAddress = (uint64_t)&IDT_TABLE;
    WriteIDTR(&idtr);
}

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
    // uint64_t address = (uint64_t)isr_stub;
    IDT_TABLE[vector].Offset_15_0 = ((uint64_t)isr_stub) & 0xFFFF;
    IDT_TABLE[vector].Selector = 0x08;
    IDT_TABLE[vector].IST = 0;
    IDT_TABLE[vector].P_DPL_0_Type = (uint8_t)(((dpl & 0x3u) << 5) | 0x8E);
    IDT_TABLE[vector].Offset_31_16 = (((uint64_t)isr_stub) >> 16) & 0xFFFF;
    IDT_TABLE[vector].Offset_63_32 = (((uint64_t)isr_stub) >> 32) & 0xFFFFFFFF;
    IDT_TABLE[vector].Reserved = 0;
}
