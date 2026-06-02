/** Main.c
 * 
 * (C) Charity Enol
 */

#include <World/World.h>
#include <HAL/HAL.h>
#include <Noyau/Enocall.h>
#include <Noyau/Memory.h>
#include <Noyau/Process.h>
#include <Drivers/Graphics.h>
#include <Drivers/VM/VMBus.h>
#include <Drivers/VM/VM_Keyboard.h>
#include <Drivers/Keyboard.h>
#include <File/File.h>
#include <UI/TextIO.h>
#include <UI/Console.h>

int64_t EVOS_STATUS = 0;

static void StopEvoncil(void);

void Evoncil(WORLD *world)
{
    InitMemory(&world->Memory);
    InitGraphics(&world->Graphics);
    InitHardware(world);
    InitProcess();
    InitEnocall();
    InitFile();
    InitKeyboard();

    EnableInterrupts();
    
    InitVMBus();
    InitVMKeyboard();

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
