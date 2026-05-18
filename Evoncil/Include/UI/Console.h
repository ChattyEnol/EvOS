/** UI/Console.h
 *
 * (C) Charity Enol
 *
 * 控制台接口。
 * 它负责提示符、命令缓冲和命令执行。
 */

#ifndef UI_CONSOLE_H
#define UI_CONSOLE_H

#include <World/World.h>

void InitConsole(WORLD *world);
void RunConsole(void);

#endif
