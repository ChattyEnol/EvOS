/** Noyau/Process.c
 *
 * (C) Charity Enol
 *
 * 极简进程表实现。
 */

#include <Noyau/Process.h>

#include <stddef.h>
#include <string.h>

#define PROCESS_TABLE_SIZE 64

static PROCESS PROCESS_TABLE[PROCESS_TABLE_SIZE];
static PROCESS_ID NEXT_PROCESS_ID = 1;
static PROCESS *CURRENT_PROCESS = NULL;

static void ClearProcess(PROCESS *process);
static void CopyProcessName(char *destination, const char *source);

void InitProcess(void)
{
    for (uint32_t index = 0; index < PROCESS_TABLE_SIZE; index++)
        ClearProcess(&PROCESS_TABLE[index]);

    NEXT_PROCESS_ID = 1;
    CURRENT_PROCESS = NULL;

    PROCESS_ID kernelProcess = CreateProcess("Evoncil", NULL, NULL);
    CURRENT_PROCESS = GetProcess(kernelProcess);
    if (CURRENT_PROCESS != NULL)
        CURRENT_PROCESS->State = PROCESS_RUNNING;
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
    return true;
}

static void ClearProcess(PROCESS *process)
{
    process->ProcessID = 0;
    process->State = PROCESS_UNUSED;
    process->Entry = NULL;
    process->Context = NULL;

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
