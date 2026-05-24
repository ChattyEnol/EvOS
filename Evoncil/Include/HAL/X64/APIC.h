/** HAL/X64/APIC.h
 *
 * (C) Charity Enol
 *
 * X64 高级可编程中断控制器头文件。
 */

#ifndef HAL_X64_APIC_H
#define HAL_X64_APIC_H

#include <stdint.h>

void InitAPIC(void *acpi_root);
void ApicEndOfInterrupt(void);

uint32_t ApicGetLocalId(void);
uint32_t ApicGetMsiAddress(void);
uint32_t ApicGetMsiData(uint8_t vector);

#endif // HAL_X64_APIC_H