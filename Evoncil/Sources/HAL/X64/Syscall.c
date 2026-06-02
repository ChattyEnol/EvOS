/** HAL/X64/Syscall.c
 *
 * (C) Charity Enol
 *
 * X64 高性能系统调用入口配置。
 */

#include <HAL/HAL.h>
#include <HAL/X64/Registers.h>
#include <HAL/X64/Table/GDT.h>

#include <stddef.h>

#define IA32_EFER_MSR 0xC0000080
#define IA32_STAR_MSR 0xC0000081
#define IA32_LSTAR_MSR 0xC0000082
#define IA32_FMASK_MSR 0xC0000084

#define IA32_EFER_SYSCALL_ENABLE (1ull << 0)
#define X64_RFLAGS_TRAP_FLAG (1ull << 8)
#define X64_RFLAGS_INTERRUPT_FLAG (1ull << 9)
#define X64_RFLAGS_DIRECTION_FLAG (1ull << 10)
#define X64_RFLAGS_ALIGNMENT_CHECK (1ull << 18)
#define ENOL_ENOSYS (-38)

// syscall 指令进入内核后的第一条汇编入口。
extern void SyscallStub(void);
// 当前内核注册的系统调用处理器。
static HandleSystemCall SystemCallHandler = NULL;
// 配置 syscall/sysret 需要使用的模型特定寄存器。
static void SetSyscallMSR(void);

// 注册平台无关的系统调用处理器，并打开 x64 syscall 指令入口。
void InitSystemCall(HandleSystemCall handler)
{
    SystemCallHandler = handler;
    SetSyscallMSR();
}

// 把 x64 syscall 寄存器约定整理成 HAL 的通用系统调用现场。
uint64_t HandleSyscall(
    uint64_t number,
    uint64_t argument0,
    uint64_t argument1,
    uint64_t argument2,
    uint64_t argument3,
    uint64_t argument4,
    uint64_t argument5)
{
    SYSTEM_CALL_CONTEXT context;

    context.Number = number;
    context.Arguments[0] = argument0;
    context.Arguments[1] = argument1;
    context.Arguments[2] = argument2;
    context.Arguments[3] = argument3;
    context.Arguments[4] = argument4;
    context.Arguments[5] = argument5;
    context.Result = (uint64_t)ENOL_ENOSYS;

    if (SystemCallHandler != NULL)
        context.Result = SystemCallHandler(&context);

    return context.Result;
}

static void SetSyscallMSR(void)
{
    uint64_t efer = ReadMSR(IA32_EFER_MSR);
    efer |= IA32_EFER_SYSCALL_ENABLE;
    WriteMSR(IA32_EFER_MSR, efer);

    uint64_t star = ((uint64_t)GD_SELECTOR_KERNEL_CODE << 32) |
                    ((uint64_t)(GD_SELECTOR_USER_DATA - 8) << 48);
    WriteMSR(IA32_STAR_MSR, star);
    WriteMSR(IA32_LSTAR_MSR, (uint64_t)SyscallStub);

    uint64_t maskedFlags = X64_RFLAGS_TRAP_FLAG |
                           X64_RFLAGS_INTERRUPT_FLAG |
                           X64_RFLAGS_DIRECTION_FLAG |
                           X64_RFLAGS_ALIGNMENT_CHECK;
    WriteMSR(IA32_FMASK_MSR, maskedFlags);
}
