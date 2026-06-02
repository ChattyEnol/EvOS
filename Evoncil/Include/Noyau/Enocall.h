/** Noyau/Enocall.h
 *
 * (C) Charity Enol
 *
 * 内核系统调用入口。
 */

#ifndef NOYAU_ENOCALL_H
#define NOYAU_ENOCALL_H

#include <stdint.h>

typedef enum
{
    ENOCALL_READ = 0,
    ENOCALL_WRITE = 1,
    ENOCALL_YIELD = 24,
    ENOCALL_GETPID = 39,
    ENOCALL_EXIT = 60
} ENOCALL_NUMBER;

/**
 * 初始化 EvOS 的系统调用分发器。
 * HAL 会负责安装当前平台真正使用的系统调用入口。
 */
void InitEnocall(void);

#endif
