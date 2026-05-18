#ifndef INFO_H
#define INFO_H

#include <EvLoader.h>

typedef enum
{
    MENU_FIRMWARE = 0,
    // MENU_MEMORY,
    MENU_GRAPHICS,
    MENU_BOOT,
    MENU_MAX // 这个用来方便计算选项总数
} MENU_OPTION;

MENU_OPTION Menu(void);

#endif