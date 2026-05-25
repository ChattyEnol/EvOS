/** HAL/X64/Interrupt.c
 *
 * (C) Charity Enol
 *
 * X64 中断分发。
 */

#include <HAL/HAL.h>
#include <HAL/X64/APIC.h>
#include <HAL/X64/Interrupt.h>
#include <HAL/X64/Table/IDT.h>
// #include <Noyau/Enolcall.h>

#include <UI/TextIO.h>

#include <stddef.h>

// 准备一个数组，存放业务层的 C 处理函数。
static HandleInterrupt InterruptHandlers[256] = {0};

void EnableInterrupts(void) { __asm__ volatile("sti"); }
void DisableInterrupts(void) { __asm__ volatile("cli"); }
void SetInterruptHandler(uint8_t vector, void *handler) { InterruptHandlers[vector] = (HandleInterrupt)handler; }

void InitInterrupt(void *acpi_root)
{
    DisableInterrupts();
    InitIDT();
    InitAPIC(acpi_root); // 硬中断！
}

// 通用分发中心（这个函数会被 `ISR.s` 里的汇编调用）。
void CommonInterruptHandler(uint64_t vector, uint64_t error_code, INTERRUPT_FRAME *frame)
{
    (void)error_code;

    if (vector == 128)
    {
        // EnolCaller(frame);
        return;
    }

    // 只有 CPU 保留的 0..31 才是异常，32 以上都是软件或外部中断。
    if (vector < 32)
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

    if (vector >= 32)
    {
        if (InterruptHandlers[vector] != NULL)
            InterruptHandlers[vector]();
        if (vector != 255)
            ApicEndOfInterrupt();
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
