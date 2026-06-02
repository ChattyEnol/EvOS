/** Noyau/Enocall.c
 *
 * (C) Charity Enol
 *
 * POSIX 风格的极简系统调用分发器。
 */

#include <Noyau/Process.h>
#include <Noyau/Enocall.h>
#include <HAL/HAL.h>
#include <UI/TextIO.h>

#include <stddef.h>
#include <stdint.h>

#define ENOL_STDIN 0
#define ENOL_STDOUT 1
#define ENOL_STDERR 2

#define ENOL_EINVAL (-22)
#define ENOL_EBADF (-9)
#define ENOL_ENOSYS (-38)

static uint64_t HandleEnocall(SYSTEM_CALL_CONTEXT *context);        // 根据系统调用号分发到具体的 Enol 系列内核实现。
static int64_t EnoRead(int fd, void *buffer, uint64_t size);        // 处理 read 系统调用。
static int64_t EnoWrite(int fd, const void *buffer, uint64_t size); // 处理 write 系统调用。
static int64_t EnoExit(int status);                                 // 处理 exit 系统调用。
static int64_t EnoGetPID(void);                                     // 处理 getpid 系统调用。
static int64_t EnoYield(void);                                      // 处理 yield 系统调用。

// 把内核的系统调用分发器注册给 HAL。
void InitEnocall(void) { InitSystemCall(HandleEnocall); }

static uint64_t HandleEnocall(SYSTEM_CALL_CONTEXT *context)
{
    if (context == NULL)
        return (uint64_t)ENOL_EINVAL;

    int64_t result;

    switch ((ENOCALL_NUMBER)context->Number)
    {
    case ENOCALL_READ:
        result = EnoRead((int)context->Arguments[0], (void *)(uintptr_t)context->Arguments[1], context->Arguments[2]);
        break;
    case ENOCALL_WRITE:
        result = EnoWrite((int)context->Arguments[0], (const void *)(uintptr_t)context->Arguments[1], context->Arguments[2]);
        break;
    case ENOCALL_YIELD:
        result = EnoYield();
        break;
    case ENOCALL_GETPID:
        result = EnoGetPID();
        break;
    case ENOCALL_EXIT:
        result = EnoExit((int)context->Arguments[0]);
        break;
    default:
        result = ENOL_ENOSYS;
        break;
    }

    context->Result = (uint64_t)result;
    return context->Result;
}

static int64_t EnoRead(int fd, void *buffer, uint64_t size)
{
    if (fd != ENOL_STDIN)
        return ENOL_EBADF;

    if (buffer == NULL || size == 0)
        return ENOL_EINVAL;

    TEXT_INPUT input;
    if (!ReadTextInput(&input) || input.Character == '\0')
        return 0;

    ((char *)buffer)[0] = input.Character;
    return 1;
}

static int64_t EnoWrite(int fd, const void *buffer, uint64_t size)
{
    if (fd != ENOL_STDOUT && fd != ENOL_STDERR)
        return ENOL_EBADF;

    if (buffer == NULL && size != 0)
        return ENOL_EINVAL;

    const char *text = (const char *)buffer;
    for (uint64_t index = 0; index < size; index++)
        TextPutChar(text[index]);

    return (int64_t)size;
}

static int64_t EnoExit(int status)
{
    (void)status;

    PROCESS_ID processID = GetCurrentProcessID();
    if (processID != 0)
        SetProcessState(processID, PROCESS_STOPPED);

    YieldProcess();
    return 0;
}

static int64_t EnoGetPID(void)
{
    return (int64_t)GetCurrentProcessID();
}

static int64_t EnoYield(void)
{
    YieldProcess();
    return 0;
}
