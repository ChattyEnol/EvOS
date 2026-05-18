/** Noyau/Process.c
 *
 * (C) Charity Enol
 *
 * 极简进程表实现。
 */

#include <Noyau/Process.h>
#include <HAL/HAL.h>

#include <stddef.h>
#include <string.h>

#define PROCESS_TABLE_SIZE 64
#define PROCESS_DEFAULT_TIME_SLICE 5

static PROCESS PROCESS_TABLE[PROCESS_TABLE_SIZE];
static PROCESS_ID NEXT_PROCESS_ID = 1;
static PROCESS *CURRENT_PROCESS = NULL;
static uint32_t CURRENT_PROCESS_INDEX = 0;

static void ClearProcess(PROCESS *process);
static void CopyProcessName(char *destination, const char *source);
static int32_t GetProcessIndex(PROCESS *process);
static bool IsSchedulableProcess(const PROCESS *process);
static void ProcessTimerInterruptHandler(void);

void InitProcess(void)
{
    for (uint32_t index = 0; index < PROCESS_TABLE_SIZE; index++)
        ClearProcess(&PROCESS_TABLE[index]);

    NEXT_PROCESS_ID = 1;
    CURRENT_PROCESS = NULL;
    CURRENT_PROCESS_INDEX = 0;

    PROCESS_ID kernelProcess = CreateProcess("Evoncil", NULL, NULL);
    CURRENT_PROCESS = GetProcess(kernelProcess);
    if (CURRENT_PROCESS != NULL)
    {
        CURRENT_PROCESS->State = PROCESS_RUNNING;
        CURRENT_PROCESS_INDEX = (uint32_t)GetProcessIndex(CURRENT_PROCESS);
    }

    SetInterruptGate(32, ProcessTimerInterruptHandler);
}

PROCESS_ID CreateProcess(const char *name, PROCESS_ENTRY entry, void *context)
{
    for (uint32_t index = 0; index < PROCESS_TABLE_SIZE; index++)
    {
        PROCESS *process = &PROCESS_TABLE[index];
        if (process->State != PROCESS_UNUSED)
            continue;

        process->ProcessID = NEXT_PROCESS_ID++;
        process->State = PROCESS_READY;
        process->Entry = entry;
        process->Context = context;
        process->TimeSlice = PROCESS_DEFAULT_TIME_SLICE;
        process->Ticks = 0;
        CopyProcessName(process->Name, name);
        return process->ProcessID;
    }

    return 0;
}

PROCESS *GetProcess(PROCESS_ID process_id)
{
    if (process_id == 0)
        return NULL;

    for (uint32_t index = 0; index < PROCESS_TABLE_SIZE; index++)
    {
        PROCESS *process = &PROCESS_TABLE[index];
        if (process->State != PROCESS_UNUSED &&
            process->ProcessID == process_id)
            return process;
    }

    return NULL;
}

PROCESS *GetCurrentProcess(void)
{
    return CURRENT_PROCESS;
}

PROCESS_ID GetCurrentProcessID(void)
{
    if (CURRENT_PROCESS == NULL)
        return 0;

    return CURRENT_PROCESS->ProcessID;
}

uint32_t GetProcessCount(void)
{
    uint32_t count = 0;

    for (uint32_t index = 0; index < PROCESS_TABLE_SIZE; index++)
        if (PROCESS_TABLE[index].State != PROCESS_UNUSED)
            count++;

    return count;
}

bool SetProcessState(PROCESS_ID process_id, PROCESS_STATE state)
{
    PROCESS *process = GetProcess(process_id);
    if (process == NULL)
        return false;

    process->State = state;
    if (process == CURRENT_PROCESS && state != PROCESS_RUNNING)
        ScheduleProcess();

    return true;
}

void YieldProcess(void)
{
    ScheduleProcess();
}

void ScheduleProcess(void)
{
    uint32_t startIndex = CURRENT_PROCESS_INDEX;

    if (CURRENT_PROCESS != NULL && CURRENT_PROCESS->State == PROCESS_RUNNING)
    {
        CURRENT_PROCESS->State = PROCESS_READY;
        CURRENT_PROCESS->Ticks = 0;
    }

    for (uint32_t offset = 1; offset <= PROCESS_TABLE_SIZE; offset++)
    {
        uint32_t index = (startIndex + offset) % PROCESS_TABLE_SIZE;
        PROCESS *process = &PROCESS_TABLE[index];
        if (!IsSchedulableProcess(process))
            continue;

        CURRENT_PROCESS = process;
        CURRENT_PROCESS_INDEX = index;
        CURRENT_PROCESS->State = PROCESS_RUNNING;
        CURRENT_PROCESS->Ticks = 0;
        return;
    }

    if (CURRENT_PROCESS != NULL && CURRENT_PROCESS->State == PROCESS_READY)
        CURRENT_PROCESS->State = PROCESS_RUNNING;
}

void TickProcess(void)
{
    if (CURRENT_PROCESS == NULL || CURRENT_PROCESS->State != PROCESS_RUNNING)
    {
        ScheduleProcess();
        return;
    }

    CURRENT_PROCESS->Ticks++;
    if (CURRENT_PROCESS->Ticks >= CURRENT_PROCESS->TimeSlice)
        ScheduleProcess();
}

static void ClearProcess(PROCESS *process)
{
    process->ProcessID = 0;
    process->State = PROCESS_UNUSED;
    process->Entry = NULL;
    process->Context = NULL;
    process->TimeSlice = PROCESS_DEFAULT_TIME_SLICE;
    process->Ticks = 0;

    for (uint32_t index = 0; index < PROCESS_NAME_SIZE; index++)
        process->Name[index] = '\0';
}

static void CopyProcessName(char *destination, const char *source)
{
    if (source == NULL)
        source = "process";

    uint32_t index = 0;
    for (; index + 1 < PROCESS_NAME_SIZE && source[index] != '\0'; index++)
        destination[index] = source[index];

    destination[index] = '\0';
}

static int32_t GetProcessIndex(PROCESS *process)
{
    for (uint32_t index = 0; index < PROCESS_TABLE_SIZE; index++)
        if (&PROCESS_TABLE[index] == process)
            return (int32_t)index;

    return -1;
}

static bool IsSchedulableProcess(const PROCESS *process)
{
    return process != NULL && process->State == PROCESS_READY;
}

static void ProcessTimerInterruptHandler(void)
{
    TickProcess();
}
