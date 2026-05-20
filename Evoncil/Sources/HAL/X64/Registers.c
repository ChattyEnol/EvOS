/** HAL/X64/Registers.c
 * 
 * (C) Charity Enol
 * 
 * X64 特殊寄存器读写。
 */

#include <HAL/HAL.h>
#include <HAL/X64/Registers.h>

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
