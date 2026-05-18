/** Main.c
 * 
 * (C) Charity Enol
 */

#include <World/World.h>
#include <HAL/HAL.h>
#include <Noyau/Memory.h>
#include <Noyau/Process.h>
#include <File/File.h>
#include <Drivers/Graphics.h>
#include <Drivers/Keyboard.h>
#include <HAL/PCIe/PCIe.h>
#include <HAL/PCIe/xHCI/xHCI.h>
#include <UI/TextIO.h>
#include <UI/Console.h>

static void StopEvoncil(void);

void Evoncil(WORLD *world)
{
    InitMemory(&world->Memory);
    InitGraphics(&world->Graphics);
    InitInterrupt(world);
    InitPCIe(world->AcpiRoot);
    InitProcess();
    InitFile();
    InitKeyboard();
    InitXhci();
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
