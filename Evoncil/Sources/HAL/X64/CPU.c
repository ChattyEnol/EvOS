/** HAL/X64/CPU.c
 *
 * (C) Charity Enol
 *
 * AMD64 处理器基础控制。
 */

#include <HAL/HAL.h>

void Halt(void) { __asm__ volatile("hlt"); }

uint64_t GetPageTableRoot(void)
{
    uint64_t value;
    __asm__ volatile("movq %%cr3, %0" : "=r"(value));
    return value;
}

void SetPageTableRoot(uint64_t root)
{
    __asm__ volatile("movq %0, %%cr3" ::"r"(root) : "memory");
}

void InvalidateTLB(void *virtual_address)
{
    __asm__ volatile("invlpg (%0)" ::"r"(virtual_address) : "memory");
}

void FlushTLB(void)
{
    SetPageTableRoot(GetPageTableRoot());
}

void EnableInterrupts(void) { __asm__ volatile("sti"); }
void DisableInterrupts(void) { __asm__ volatile("cli"); }

uint64_t ReadMSR(uint32_t msr_id)
{
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr_id));
    return ((uint64_t)high << 32) | low;
}

void WriteMSR(uint32_t msr_id, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = (value >> 32) & 0xFFFFFFFF;
    __asm__ volatile("wrmsr" ::"c"(msr_id), "a"(low), "d"(high));
}

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

uint64_t ReadControlRegister(uint32_t reg_num)
{
    uint64_t value = 0;

    switch (reg_num)
    {
    case 0:
        __asm__ volatile("movq %%cr0, %0" : "=r"(value));
        break;
    case 2:
        __asm__ volatile("movq %%cr2, %0" : "=r"(value));
        break;
    case 3:
        __asm__ volatile("movq %%cr3, %0" : "=r"(value));
        break;
    case 4:
        __asm__ volatile("movq %%cr4, %0" : "=r"(value));
        break;
    }

    return value;
}

void WriteControlRegister(uint32_t reg_num, uint64_t value)
{
    switch (reg_num)
    {
    case 0:
        __asm__ volatile("movq %0, %%cr0" ::"r"(value) : "memory");
        break;
    case 2:
        __asm__ volatile("movq %0, %%cr2" ::"r"(value) : "memory");
        break;
    case 3:
        __asm__ volatile("movq %0, %%cr3" ::"r"(value) : "memory");
        break;
    case 4:
        __asm__ volatile("movq %0, %%cr4" ::"r"(value) : "memory");
        break;
    }
}
