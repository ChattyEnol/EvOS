/** HAL/X64/IO.c
 *
 * (C) Charity Enol
 *
 * X64 端口 I/O。
 */

#include <HAL/HAL.h>
#include <HAL/X64/IO.h>

uint8_t ReadHardwarePortByte(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void WriteHardwarePortByte(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" ::"a"(value), "Nd"(port));
}
