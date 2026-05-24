/** EvOS.h
 *
 * (C) Charity Enol
 *
 * 调试和基本信息用。
 */

#ifndef EVOS_H
#define EVOS_H

#include <HAL/HAL.h>
#include <UI/TextIO.h>
#include <stdint.h>

extern int64_t EVOS_STATUS;

/**
 * EV_OPT 宏：包裹内核关键操作
 * 如果操作返回值不为 0（代表发生错误），则触发内核恐慌（Panic），打印详细错误并挂起 CPU
 */
#define EV_OPT(command)                                                     \
    do                                                                      \
    {                                                                       \
        EVOS_STATUS = (int64_t)(command);                                   \
        if (EVOS_STATUS != 0)                                               \
        {                                                                   \
            kprintf("\n[EvOS Kernel Panic]\n");                             \
            kprintf("Location: %s:%d\n", __FILE__, __LINE__);               \
            kprintf("Command:  %s\n", #command);                            \
            kprintf("Status:   %p\n", (unsigned long long)EVOS_STATUS); \
            while (1)                                                       \
                Halt();                                                     \
        }                                                                   \
    } while (0)

/**
 * EV_OPT_BYPASS 宏：允许绕过特定预期结果的内核操作包裹宏
 * 如果操作返回值既不为 0，又不等于指定的 bypass 状态，则触发内核恐慌
 */
#define EV_OPT_BYPASS(command, error_bypass)                            \
    do                                                                  \
    {                                                                   \
        EVOS_STATUS = (int64_t)(command);                               \
        if (EVOS_STATUS != 0 && EVOS_STATUS != (int64_t)(error_bypass)) \
        {                                                               \
            kprintf("\n[EvOS Unexpected Kernel Error]\n");              \
            kprintf("Location: %s:%d\n", __FILE__, __LINE__);           \
            kprintf("Command:  %s\n", #command);                        \
            kprintf("Status:   %p\n", (unsigned long long)EVOS_STATUS); \
            while (1)                                                   \
                Halt();                                                 \
        }                                                               \
    } while (0)

#endif