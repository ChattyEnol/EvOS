/** HAL/X64/IO.h
 *
 * (C) Charity Enol
 *
 * I/O 端口读写。
 */

#ifndef HAL_X64_IO_H
#define HAL_X64_IO_H

#include <HAL/HAL.h>
#include <stdint.h>

uint8_t ReadHardwarePortByte(uint16_t port);
void WriteHardwarePortByte(uint16_t port, uint8_t value);

#endif // HAL_X64_IO_H
