/** HAL/X64/Page.c
 *
 * (C) Charity Enol
 *
 * X64 内存和页表相关。
 */

#include <HAL/HAL.h>

uint64_t GetPageTableRoot(void)
{
    uint64_t value;
    __asm__ volatile("movq %%cr3, %0" : "=r"(value));
    return value;
}

void SetPageTableRoot(uint64_t root) { __asm__ volatile("movq %0, %%cr3" ::"r"(root) : "memory"); }
void InvalidateTLB(void *virtual_address) { __asm__ volatile("invlpg (%0)" ::"r"(virtual_address) : "memory"); }
void FlushTLB(void) { SetPageTableRoot(GetPageTableRoot()); }
