/** HAL/X64/Hypervisor.c
 *
 * (C) Charity Enol
 *
 * X64 架构下的 Hypervisor 平台相关实现。
 */

#include <EvOS.h>
#include <HAL/HAL.h>
#include <HAL/X64/Registers.h>
#include <Drivers/VM/VMBus.h>
#include <Noyau/Memory.h>

#define HV_X64_MSR_GUEST_OS_ID 0x40000000 // 微软规定的身份登记寄存器
#define HV_X64_MSR_HYPERCALL 0x40000001   // Hypercall 启用寄存器
#define HV_X64_MSR_SCONTROL 0x40000080    // 合成中断控制寄存器
#define HV_X64_MSR_SVERSION 0x40000081
#define HV_X64_MSR_SIEFP 0x40000082 // 事件通知基础设施映射寄存器
#define HV_X64_MSR_SIMP 0x40000083  // 消息基础设施映射寄存器
#define HV_X64_MSR_EOM 0x40000084
#define HV_X64_MSR_SINT0 0x40000090
#define HV_X64_MSR_SINT2 0x40000092

static uint64_t EVOS_ID = 0x8100000045764F53ull;
static void *HypercallPage = NULL;

typedef uint64_t (*HYPERCALL_PAGE_ROUTINE)(uint64_t control, uint64_t input, uint64_t output);

void SetupHypervisor(uint64_t message_address, uint64_t event_address)
{
    WriteMSR(HV_X64_MSR_GUEST_OS_ID, EVOS_ID);

    // 必须先分配并激活 Hypercall 页面，否则所有的 `vmcall` 都会失效。
    HypercallPage = AllocatePage(4096);
    uint64_t hypercallAddress = GetPhysicalAddress(HypercallPage);
    // Bit 0 是启用位（Enable），把它置 1。
    WriteMSR(HV_X64_MSR_HYPERCALL, hypercallAddress | 1);

    // 把完整的物理地址打入特定的 MSR 中，最后的 1 代表启用该通道。
    WriteMSR(HV_X64_MSR_SIMP, (message_address) | 1);
    WriteMSR(HV_X64_MSR_SIEFP, (event_address) | 1);
    // 写入向量号（80），保持 SINT2 未屏蔽。EOI 仍然交给 Local APIC 和 EOM 正常处理。
    WriteMSR(HV_X64_MSR_SINT2, VMBUS_INTERRUPT_VECTOR);
    // 激活整个合成中断控制面。
    WriteMSR(HV_X64_MSR_SCONTROL, 1);
}

uint64_t Hypercall(uint64_t control_code, uint64_t input_parameter)
{
    uint64_t result;

    if (HypercallPage == NULL)
        return 0xFFFFFFFFFFFFFFFFull;

    HYPERCALL_PAGE_ROUTINE routine = (HYPERCALL_PAGE_ROUTINE)HypercallPage;
    result = routine(control_code, input_parameter, 0);

    return result;
}

void AckHyperMessage(void)
{
    // 在 x64 架构下，我们通过向 EOM (End of Message) 寄存器写入 0 来拉低响应线。
    // 告诉 Hyper-V 这个槽位空出来了，可以派发下一个控制中断了。
    WriteMSR(HV_X64_MSR_EOM, 0);
}
