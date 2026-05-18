/** Noyau/Process.h
 *
 * (C) Charity Enol
 *
 * 极简进程管理。
 * 现在先记录进程表，之后再接调度器和地址空间。
 */

#ifndef NOYAU_PROCESS_H
#define NOYAU_PROCESS_H

#include <stdbool.h>
#include <stdint.h>

#define PROCESS_NAME_SIZE 32

typedef uint32_t PROCESS_ID;

typedef enum
{
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_WAITING,
    PROCESS_STOPPED
} PROCESS_STATE;

typedef void (*PROCESS_ENTRY)(void *);

typedef struct
{
    PROCESS_ID ProcessID;
    PROCESS_STATE State;
    char Name[PROCESS_NAME_SIZE];
    PROCESS_ENTRY Entry;
    void *Context;
} PROCESS;

void InitProcess(void);
PROCESS_ID CreateProcess(const char *name, PROCESS_ENTRY entry, void *context);
PROCESS *GetProcess(PROCESS_ID process_id);
PROCESS *GetCurrentProcess(void);
uint32_t GetProcessCount(void);
bool SetProcessState(PROCESS_ID process_id, PROCESS_STATE state);

#endif // NOYAU_PROCESS_H
