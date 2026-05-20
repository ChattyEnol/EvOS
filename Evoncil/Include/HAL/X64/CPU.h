/** HAL/X64/CPU.h
 *
 * (C) Charity Enol
 *
 * CPU 基本信息。
 */

#ifndef HAL_X64_CPU_H
#define HAL_X64_CPU_H

#include <HAL/HAL.h>
#include <stdint.h>

/**
 * 执行这条指令能让内核知道当前这颗处理器到底是 Intel 还是 AMD，
 * 支持什么特性（比如有没有 AVX 指令集、支持多少个核心等）。
 */
void ReadCPUID(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx);

#endif // HAL_X64_CPU_H
