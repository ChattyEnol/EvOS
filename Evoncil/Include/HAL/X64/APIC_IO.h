/** HAL/X64/APIC_IO.h
 *
 * (C) Charity Enol
 *
 * X64 高级可编程中断控制器 IO 端头文件。
 */

#ifndef HAL_X64_APIC_IO_H
#define HAL_X64_APIC_IO_H

#include <stdint.h>

void ApicIoInit(void);
void ApicIoReset(void);
void ApicIoAddChip(uint64_t physical_address, uint32_t vector_base);
void ApicIoAddOverride(uint8_t source, uint32_t vector, uint16_t flags);

#endif // HAL_X64_APIC_IO_H