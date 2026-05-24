/** UI/Console.c
 *
 * (C) Charity Enol
 *
 * 控制台实现。
 * 它只管欢迎页、提示符、命令缓冲和命令执行。
 */

#include <HAL/HAL.h>
// #include <HAL/PCIe/PCIe.h>
// #include <HAL/PCIe/xHCI/xHCI.h>

#include <Noyau/Memory.h>
#include <Noyau/Process.h>
#include <File/File.h>
#include <UI/Console.h>
#include <UI/TextIO.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define COMMAND_BUFFER_SIZE 128

static WORLD *CONSOLE_WORLD = NULL;
static char COMMAND_BUFFER[COMMAND_BUFFER_SIZE];
static uint32_t COMMAND_LENGTH = 0;

static void PrintHeader(void);
static void PrintPrompt(void);
static void PrintHelp(void);
static void PrintInfo(void);
static void PrintProcess(void);
static void PrintFileSystem(void);
static void ClearConsole(void);
static bool ExecuteCommand(void);
static void AppendCommandCharacter(char character);
static void RemoveCommandCharacter(void);
static void ClearCommandBuffer(void);
static bool CommandEquals(const char *command);
static bool CommandStartsWith(const char *prefix);
static const char *SkipSpaces(const char *string);

void InitConsole(WORLD *world)
{
    CONSOLE_WORLD = world;
    COMMAND_LENGTH = 0;

    ClearConsole();
}

void RunConsole(void)
{
    while (1)
    {
        TEXT_INPUT input;
        if (!ReadTextInput(&input))
            continue;

        char character = input.Character;

        if (character == '\n')
        {
            TextPutChar('\n');
            COMMAND_BUFFER[COMMAND_LENGTH] = '\0';
            bool shouldPrintPrompt = ExecuteCommand();
            ClearCommandBuffer();

            if (shouldPrintPrompt)
                PrintPrompt();
            continue;
        }

        if (character == '\b')
        {
            RemoveCommandCharacter();
            continue;
        }

        if (character < ' ' || character > '~')
            continue;

        AppendCommandCharacter(character);
    }
}

static void PrintHeader(void)
{
    kprintf("Evoncil OS\n");
    kprintf("----------\n");
    kprintf("(C) Charity Enol\n");
    kprintf("\n");
}

static void PrintPrompt(void)
{
    kprintf("EvOS> ");
}

static void PrintHelp(void)
{
    kprintf("help       Show commands.\n");
    kprintf("info       Show system information.\n");
    kprintf("process    Show process information.\n");
    kprintf("fs         Show file system information.\n");
    kprintf("xhci       Show xHCI controller information.\n");
    kprintf("clear      Clear the screen.\n");
    kprintf("echo TEXT  Print TEXT.\n");
}

static void PrintInfo(void)
{
    if (CONSOLE_WORLD == NULL)
        return;

    kprintf("Graphics driver online.\n");
    kprintf("Resolution: %u x %u\n",
            CONSOLE_WORLD->Graphics.HorizontalResolution,
            CONSOLE_WORLD->Graphics.VerticalResolution);
    kprintf("Framebuffer: %p\n", (void *)(uintptr_t)CONSOLE_WORLD->Graphics.FrameBufferBase);
    kprintf("Free memory: %u KB\n", (unsigned int)(GetFreeMemorySize() / 1024));
    kprintf("Used memory: %u KB\n", (unsigned int)(GetUsedMemorySize() / 1024));
    kprintf("Processes: %u\n", GetProcessCount());
    kprintf("File system: %s\n", IsFileSystemReady() ? "FAT32" : "not mounted");
}

static void PrintProcess(void)
{
    PROCESS *currentProcess = GetCurrentProcess();

    kprintf("Process count: %u\n", GetProcessCount());
    if (currentProcess != NULL)
        kprintf("Current: %u %s\n", currentProcess->ProcessID, currentProcess->Name);
}

static void PrintFileSystem(void)
{
    FAT32_VOLUME *volume = GetSystemVolume();

    if (!IsFileSystemReady())
    {
        kprintf("FAT32 volume not mounted.\n");
        return;
    }

    kprintf("FAT32 mounted.\n");
    kprintf("Bytes/sector: %u\n", volume->BytesPerSector);
    kprintf("Sectors/cluster: %u\n", volume->SectorsPerCluster);
    kprintf("Root cluster: %u\n", volume->RootCluster);
}

static void ClearConsole(void)
{
    DrawScreen(COLOR_EVONCIL);
    InitTextIO();
    SetTextColor(COLOR_WHITE, COLOR_EVONCIL);

    PrintHeader();
    PrintPrompt();
}

static bool ExecuteCommand(void)
{
    const char *command = SkipSpaces(COMMAND_BUFFER);

    if (*command == '\0')
        return true;

    if (CommandEquals("help"))
    {
        PrintHelp();
        return true;
    }

    if (CommandEquals("clear"))
    {
        ClearConsole();
        return false;
    }

    if (CommandEquals("info"))
    {
        PrintInfo();
        return true;
    }

    if (CommandEquals("process"))
    {
        PrintProcess();
        return true;
    }

    if (CommandEquals("fs"))
    {
        PrintFileSystem();
        return true;
    }

    if (CommandStartsWith("echo"))
    {
        const char *text = SkipSpaces(command + 4);
        kprintf("%s\n", text);
        return true;
    }

    kprintf("Unknown command: %s\n", command);
    return true;
}

static void AppendCommandCharacter(char character)
{
    if (COMMAND_LENGTH + 1 >= COMMAND_BUFFER_SIZE)
        return;

    COMMAND_BUFFER[COMMAND_LENGTH] = character;
    COMMAND_LENGTH++;
    COMMAND_BUFFER[COMMAND_LENGTH] = '\0';
    TextPutChar(character);
}

static void RemoveCommandCharacter(void)
{
    if (COMMAND_LENGTH == 0)
        return;

    COMMAND_LENGTH--;
    COMMAND_BUFFER[COMMAND_LENGTH] = '\0';
    TextBackspace();
}

static void ClearCommandBuffer(void)
{
    for (uint32_t index = 0; index < COMMAND_BUFFER_SIZE; index++)
        COMMAND_BUFFER[index] = '\0';

    COMMAND_LENGTH = 0;
}

static bool CommandEquals(const char *expected)
{
    const char *command = SkipSpaces(COMMAND_BUFFER);

    return strcmp(command, expected) == 0;
}

static bool CommandStartsWith(const char *prefix)
{
    const char *command = SkipSpaces(COMMAND_BUFFER);
    uint32_t index = 0;

    while (prefix[index] != '\0')
    {
        if (command[index] != prefix[index])
            return false;

        index++;
    }

    return command[index] == '\0' || command[index] == ' ';
}

static const char *SkipSpaces(const char *string)
{
    while (*string == ' ' || *string == '\t')
        string++;

    return string;
}
