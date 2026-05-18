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
    uint32_t TimeSlice;
    uint32_t Ticks;
} PROCESS;

/**
 * 初始化进程表，并创建内核自身的初始进程记录。
 */
void InitProcess(void);

/**
 * 创建一个处于 READY 状态的新进程。
 */
PROCESS_ID CreateProcess(const char *name, PROCESS_ENTRY entry, void *context);

/**
 * 根据 PID 查找进程记录。
 */
PROCESS *GetProcess(PROCESS_ID process_id);

/**
 * 获取当前正在运行的进程记录。
 */
PROCESS *GetCurrentProcess(void);

/**
 * 获取当前正在运行的进程 PID。
 */
PROCESS_ID GetCurrentProcessID(void);

/**
 * 获取进程表中非 UNUSED 的进程数量。
 */
uint32_t GetProcessCount(void);

/**
 * 修改进程状态。
 */
bool SetProcessState(PROCESS_ID process_id, PROCESS_STATE state);

/**
 * 主动让出当前时间片。
 */
void YieldProcess(void);

/**
 * 执行一次轮转调度，选择下一个 READY 进程运行。
 */
void ScheduleProcess(void);

/**
 * 由时钟中断调用的调度计时入口。
 */
void TickProcess(void);

#endif // NOYAU_PROCESS_H
