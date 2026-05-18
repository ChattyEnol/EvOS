#ifndef OPTIONS_H
#define OPTIONS_H

#include <Menu.h>

// 执行菜单里选择的操作。
// 返回 `1` 表示继续留在菜单，返回 `0` 表示跳出循环启动内核。
int Opt(MENU_OPTION choice);

#endif