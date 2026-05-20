/** Main.c
 * 
 * (C) Charity Enol
 */

#include <World/World.h>
#include <HAL/HAL.h>
// #include <HAL/PCIe/PCIe.h>
// #include <HAL/PCIe/xHCI/xHCI.h>
#include <Noyau/Memory.h>
#include <Noyau/Process.h>
#include <Drivers/Graphics.h>
#include <Drivers/Keyboard.h>
#include <File/File.h>
#include <UI/TextIO.h>
#include <UI/Console.h>

static void StopEvoncil(void);

void Evoncil(WORLD *world)
{
    InitMemory(&world->Memory);
    InitGraphics(&world->Graphics);
    InitHardware(world);
    InitProcess();
    InitFile();
    InitKeyboard();
    // StopEvoncil();
    /**
     * 问题在这里，只要在它之前 Stop 掉电脑就不会重启。
     * 看来中断没有处理好。
     */
    EnableInterrupts();

    DrawScreen(COLOR_EVONCIL);
    InitConsole(world);
    RunConsole();

    StopEvoncil();
}

static void StopEvoncil(void)
{
    while (1)
        Halt();
}
