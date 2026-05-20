/** HAL/X64/Interrupt.h
 *
 * (C) Charity Enol
 *
 * X64 中断控制硬件。
 * APIC / MSI，X64 的高级中断控制器。
 */

#ifndef HAL_X64_INTERRUPT_H
#define HAL_X64_INTERRUPT_H

#include <HAL/HAL.h>
#include <stdint.h>

void InitInterrupt(void *acpi_root); // 初始化平台的整个中断控制系统。
void InitAPIC(void *acpi_root);
uint32_t GetLocalAPICID(void);
uint32_t GetMSIMessageAddress(void);
uint32_t GetMSIMessageData(uint8_t vector);

#endif // HAL_X64_INTERRUPT_H
