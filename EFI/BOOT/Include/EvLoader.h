#ifndef EV_LOADER_H
#define EV_LOADER_H

#include <Uefi.h>
#include <stdio.h>

/**
 * 以下的东西都是有关 EvLoader 的状态的。
 * `EV_LOADER_STATUS` 是加载器全局的状态，位于 `BOOTX64.c` 里。
 * `EV_LOAD` 是一个宏，用来执行 UEFI 的函数调用，并检查返回值是否是错误。
 */

extern EFI_STATUS EV_LOADER_STATUS;

#define EV_LOAD(command)                                               \
    do                                                                 \
    {                                                                  \
        EV_LOADER_STATUS = (command);                                  \
        if (EFI_ERROR(EV_LOADER_STATUS))                               \
        {                                                              \
            EFI_STATUS ERROR_CODE = EV_LOADER_STATUS;                  \
            SYSTEM_TABLE->ConOut->SetAttribute(                        \
                SYSTEM_TABLE->ConOut, EFI_WHITE | EFI_BACKGROUND_RED); \
            SYSTEM_TABLE->ConOut->ClearScreen(SYSTEM_TABLE->ConOut);   \
            printf("\r\n[EvLoader Error]\r\n");                        \
            printf("Location: %s:%d\r\n", __FILE__, __LINE__);         \
            printf("Command: %s\r\n", #command);                       \
            printf("Status: 0x%x\r\n", ERROR_CODE);                    \
            while (1)                                                  \
                __asm__("hlt");                                        \
        }                                                              \
    } while (0)

#define EV_LOAD_BYPASS(command, error_bypass)                          \
    do                                                                 \
    {                                                                  \
        EV_LOADER_STATUS = (command);                                  \
        if (EV_LOADER_STATUS != error_bypass)                          \
        {                                                              \
            EFI_STATUS ERROR_CODE = EV_LOADER_STATUS;                  \
            SYSTEM_TABLE->ConOut->SetAttribute(                        \
                SYSTEM_TABLE->ConOut, EFI_WHITE | EFI_BACKGROUND_RED); \
            SYSTEM_TABLE->ConOut->ClearScreen(SYSTEM_TABLE->ConOut);   \
            printf("\r\n[EvLoader Unexpected Error]\r\n");             \
            printf("Location: %s:%d\r\n", __FILE__, __LINE__);         \
            printf("Command: %s\r\n", #command);                       \
            printf("Status: 0x%x\r\n", ERROR_CODE);                    \
            while (1)                                                  \
                __asm__("hlt");                                        \
        }                                                              \
    } while (0)

/**
 * 这些东西是初始化各个模块用的。
 * 简单来说，就是传递 `SystemTable`。
 * 实现在 `Initialize.c` 里。
 */

extern EFI_SYSTEM_TABLE *SYSTEM_TABLE;
extern EFI_BOOT_SERVICES *BOOT_SERVICES;

void Initialize(EFI_SYSTEM_TABLE *SystemTable);
void Deinitialize(void);

#endif // EV_LOADER_H