/** HAL/X64/CPU.c
 *
 * (C) Charity Enol
 *
 * X64 处理器基础控制。
 */

#include <World/World.h>

#include <HAL/HAL.h>

#include <HAL/X64/CPU.h>
#include <HAL/X64/Interrupt.h>
#include <HAL/X64/Table/GDT.h>
#include <HAL/X64/Table/IDT.h>

// #include <HAL/PCIe/PCIe.h>
// #include <HAL/PCIe/xHCI/xHCI.h>

void InitHardware(WORLD *world)
{
    InitGDT();
    InitInterrupt(world->AcpiRoot);
    // InitPCIe(world->AcpiRoot);
    // InitXhci();
}

void Halt(void) { __asm__ volatile("hlt"); }
void Pause(void) { __asm__ volatile("pause"); }

void ReadCPUID(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx)
{
    uint32_t eaxValue;
    uint32_t ebxValue;
    uint32_t ecxValue;
    uint32_t edxValue;

    __asm__ volatile(
        "cpuid"
        : "=a"(eaxValue), "=b"(ebxValue), "=c"(ecxValue), "=d"(edxValue)
        : "a"(leaf), "c"(subleaf));

    if (eax != 0)
        *eax = eaxValue;
    if (ebx != 0)
        *ebx = ebxValue;
    if (ecx != 0)
        *ecx = ecxValue;
    if (edx != 0)
        *edx = edxValue;
}
