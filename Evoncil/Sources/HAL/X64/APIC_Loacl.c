/** HAL/X64/APIC_Local.c
 *
 * (C) Charity Enol
 *
 * Local APIC 初始化与控制。
 */

#include <HAL/HAL.h>
#include <HAL/ACPI.h>
#include <HAL/X64/APIC.h>
#include <HAL/X64/APIC_IO.h>
#include <HAL/X64/CPU.h>
#include <HAL/X64/Registers.h>
#include <HAL/X64/Interrupt.h>
#include <Noyau/Memory.h>

#include <stdbool.h>
#include <stddef.h>

#define IA32_APIC_BASE_MSR 0x1B         // APIC 基地址 MSR 寄存器
#define IA32_APIC_BASE_X2APIC 0x400     // MSR 中用来开启 x2APIC 的位
#define IA32_APIC_BASE_ENABLE 0x800     // MSR 中用来全局启用 APIC 的位
#define IA32_X2APIC_MSR_BASE 0x800      // x2APIC 寄存器映射的 MSR 起始号
#define CPUID_FEATURE_X2APIC (1u << 21) // CPUID 检测 x2APIC 支持的标志位

#define LOCAL_APIC_ID 0x020                  // 本地 APIC ID 寄存器偏移
#define LOCAL_APIC_EOI 0x0B0                 // 中断结束寄存器偏移
#define LOCAL_APIC_SPURIOUS 0x0F0            // 伪中断向量寄存器偏移
#define LOCAL_APIC_TPR 0x080                 // 任务优先级寄存器偏移
#define LOCAL_APIC_LVT_TIMER 0x320           // 定时器局部向量表寄存器偏移
#define LOCAL_APIC_TIMER_INITIAL_COUNT 0x380 // 定时器初始计数寄存器偏移
#define LOCAL_APIC_TIMER_DIVIDE 0x3E0        // 定时器分频配置寄存器偏移
#define LOCAL_APIC_TIMER_VECTOR 32           // 映射给定时器的中断向量号
#define LOCAL_APIC_TIMER_PERIODIC (1u << 17) // 定时器设为周期触发模式
#define LOCAL_APIC_SPURIOUS_VECTOR 255       // 伪中断向量号
#define LOCAL_APIC_MMIO_SIZE 0x1000          // Local APIC 内存映射空间大小

#define ACPI_SIGNATURE_MADT 0x43495041u // MADT 表的特殊签名 (APIC)

typedef enum
{
    APIC_MODE_NONE = 0,
    APIC_MODE_XAPIC,
    APIC_MODE_X2APIC
} APIC_MODE;

static volatile uint32_t *LOCAL_APIC = NULL;              // xAPIC 模式下的虚拟基地址指针
static APIC_MODE LOCAL_APIC_MODE = APIC_MODE_NONE;        // 当前正在运作的 APIC 模式
static uint64_t LOCAL_APIC_PHYSICAL_ADDRESS = 0xFEE00000; // 默认的 Local APIC 物理基地址

static uint32_t ReadLocalAPIC(uint32_t register_offset);
static void WriteLocalAPIC(uint32_t register_offset, uint32_t value);

void InitAPIC(void *acpi_root)
{
    // LOCAL_APIC_PHYSICAL_ADDRESS = 0xFEE00000;
    // LOCAL_APIC = NULL;
    // LOCAL_APIC_MODE = APIC_MODE_NONE;
    ApicIoReset();
    InitACPI(acpi_root);

    MADT *madt = (MADT *)AcpiFindTable(ACPI_SIGNATURE_MADT);
    if (madt != NULL)
    {
        LOCAL_APIC_PHYSICAL_ADDRESS = madt->LocalApicAddress;
        uint8_t *entry = (uint8_t *)madt + sizeof(MADT);
        uint8_t *end = (uint8_t *)madt + madt->Header.Length;

        while (entry + sizeof(MADT_ENTRY_HEADER) <= end)
        {
            MADT_ENTRY_HEADER *header = (MADT_ENTRY_HEADER *)entry;
            if (header->Length == 0 || entry + header->Length > end)
                break;

            if (header->Type == 1)
            {
                MADT_IO_APIC *ioApic = (MADT_IO_APIC *)entry;
                ApicIoAddChip(ioApic->IOApicAddress, ioApic->VectorBase);
            }
            else if (header->Type == 2)
            {
                MADT_INTERRUPT_SOURCE_OVERRIDE *ovr = (MADT_INTERRUPT_SOURCE_OVERRIDE *)entry;
                ApicIoAddOverride(ovr->Source, ovr->Vector, ovr->Flags);
            }
            else if (header->Type == 5)
            {
                MADT_LOCAL_APIC_ADDRESS_OVERRIDE *ovr = (MADT_LOCAL_APIC_ADDRESS_OVERRIDE *)entry;
                LOCAL_APIC_PHYSICAL_ADDRESS = ovr->LocalApicAddress;
            }
            entry += header->Length;
        }
    }

    uint64_t apicBase = ReadMSR(IA32_APIC_BASE_MSR);
    uint32_t ecx = 0;
    ReadCPUID(1, 0, NULL, NULL, &ecx, NULL);

    if ((ecx & CPUID_FEATURE_X2APIC) != 0)
    {
        apicBase |= IA32_APIC_BASE_ENABLE | IA32_APIC_BASE_X2APIC;
        WriteMSR(IA32_APIC_BASE_MSR, apicBase);
        LOCAL_APIC_MODE = APIC_MODE_X2APIC;
    }
    else
    {
        apicBase &= (uint64_t)~IA32_APIC_BASE_X2APIC;
        apicBase &= 0xFFF;
        apicBase |= (LOCAL_APIC_PHYSICAL_ADDRESS & 0xFFFFFFFFFFFFF000ull);
        apicBase |= IA32_APIC_BASE_ENABLE;
        WriteMSR(IA32_APIC_BASE_MSR, apicBase);

        LOCAL_APIC = (volatile uint32_t *)MapDeviceMemory(LOCAL_APIC_PHYSICAL_ADDRESS, LOCAL_APIC_MMIO_SIZE);
        if (LOCAL_APIC == NULL)
            return;
        LOCAL_APIC_MODE = APIC_MODE_XAPIC;
    }

    WriteLocalAPIC(LOCAL_APIC_TPR, 0);
    WriteLocalAPIC(LOCAL_APIC_SPURIOUS, ReadLocalAPIC(LOCAL_APIC_SPURIOUS) | 0x100 | LOCAL_APIC_SPURIOUS_VECTOR);
    WriteLocalAPIC(LOCAL_APIC_TIMER_DIVIDE, 0x3);
    WriteLocalAPIC(LOCAL_APIC_LVT_TIMER, LOCAL_APIC_TIMER_PERIODIC | LOCAL_APIC_TIMER_VECTOR);
    WriteLocalAPIC(LOCAL_APIC_TIMER_INITIAL_COUNT, 10000000u);

    ApicIoInit();
}

void ApicEndOfInterrupt(void)
{
    if (LOCAL_APIC_MODE != APIC_MODE_NONE)
        WriteLocalAPIC(LOCAL_APIC_EOI, 0);
}

uint32_t ApicGetLocalId(void)
{
    if (LOCAL_APIC_MODE == APIC_MODE_X2APIC)
        return ReadLocalAPIC(LOCAL_APIC_ID);
    if (LOCAL_APIC_MODE == APIC_MODE_XAPIC)
        return ReadLocalAPIC(LOCAL_APIC_ID) >> 24;
    return 0;
}

uint32_t ApicGetMsiAddress(void) { return 0xFEE00000u | ((ApicGetLocalId() & 0xFF) << 12); }
uint32_t ApicGetMsiData(uint8_t vector) { return vector; }

static uint32_t ReadLocalAPIC(uint32_t register_offset)
{
    if (LOCAL_APIC_MODE == APIC_MODE_X2APIC)
        return (uint32_t)ReadMSR(IA32_X2APIC_MSR_BASE + register_offset / 0x10);
    return LOCAL_APIC ? LOCAL_APIC[register_offset / sizeof(uint32_t)] : 0;
}

static void WriteLocalAPIC(uint32_t register_offset, uint32_t value)
{
    if (LOCAL_APIC_MODE == APIC_MODE_X2APIC)
        WriteMSR(IA32_X2APIC_MSR_BASE + register_offset / 0x10, value);
    else if (LOCAL_APIC != NULL)
        LOCAL_APIC[register_offset / sizeof(uint32_t)] = value;
}