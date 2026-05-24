/** HAL/X64/APIC_IO.c
 *
 * (C) Charity Enol
 *
 * IO APIC 引脚重定向配置。
 */

#include <HAL/HAL.h>
#include <HAL/X64/APIC.h>
#include <HAL/X64/APIC_IO.h>
#include <HAL/X64/Interrupt.h>
#include <Noyau/Memory.h>

#include <stdbool.h>
#include <stddef.h>

#define IO_APIC_MMIO_SIZE 0x1000
#define IO_APIC_REGISTER_SELECT 0x00
#define IO_APIC_REGISTER_WINDOW 0x10
#define IO_APIC_VERSION 0x01
#define IO_APIC_REDIRECTION_BASE 0x10

#define IO_APIC_REDIRECTION_MASKED (1u << 16)          // 默认屏蔽中断
#define IO_APIC_REDIRECTION_ACTIVE_LOW (1u << 13)      // 低电平有效
#define IO_APIC_REDIRECTION_LEVEL_TRIGGERED (1u << 15) // 电平触发

typedef struct
{
    volatile uint32_t *Address; // 映射后的虚拟地址
    uint32_t VectorBase;        // 起始全局中断号
    uint32_t RedirectionCount;  // 引脚总数
} IO_APIC;

typedef struct
{
    uint8_t Source;  // 源 ISA 中断号
    uint32_t Vector; // 映射后的全局系统中断号 (GSI)
    uint16_t Flags;  // 触发与极性标志
} INTERRUPT_OVERRIDE;

static IO_APIC IO_APICS[8];
static uint32_t IO_APIC_COUNT = 0;
static INTERRUPT_OVERRIDE INTERRUPT_OVERRIDES[16];
static uint32_t INTERRUPT_OVERRIDE_COUNT = 0;

static IO_APIC *FindIOAPIC(uint32_t global_system_interrupt);
static uint32_t ReadIOAPIC(IO_APIC *io_apic, uint32_t register_index);
static void WriteIOAPIC(IO_APIC *io_apic, uint32_t register_index, uint32_t value);

void ApicIoInit(void)
{
    // 遍历系统中所有找出的 IO APIC 芯片
    for (uint32_t index = 0; index < IO_APIC_COUNT; index++)
    {
        IO_APIC *ioApic = &IO_APICS[index];
        uint32_t version = ReadIOAPIC(ioApic, IO_APIC_VERSION);
        ioApic->RedirectionCount = ((version >> 16) & 0xFF) + 1; // 从硬件寄存器获取当前芯片的真实引脚数

        // 默认初始化：把所有引脚全部拉高屏蔽，防止未准备好的硬件乱发信号导致内核崩溃
        for (uint32_t pin = 0; pin < ioApic->RedirectionCount; pin++)
        {
            WriteIOAPIC(ioApic, IO_APIC_REDIRECTION_BASE + pin * 2 + 1, 0);
            WriteIOAPIC(ioApic, IO_APIC_REDIRECTION_BASE + pin * 2, IO_APIC_REDIRECTION_MASKED);
        }
    }
}

void ApicIoReset(void)
{
    IO_APIC_COUNT = 0;
    INTERRUPT_OVERRIDE_COUNT = 0;
}

void ApicIoAddChip(uint64_t physical_address, uint32_t vector_base)
{
    if (IO_APIC_COUNT >= sizeof(IO_APICS) / sizeof(IO_APICS[0]))
        return;

    volatile uint32_t *mapped = (volatile uint32_t *)MapDeviceMemory(physical_address, IO_APIC_MMIO_SIZE);
    if (mapped == NULL)
        return;

    IO_APICS[IO_APIC_COUNT].Address = mapped;
    IO_APICS[IO_APIC_COUNT].VectorBase = vector_base;
    IO_APICS[IO_APIC_COUNT].RedirectionCount = 0;
    IO_APIC_COUNT++;
}

void ApicIoAddOverride(uint8_t source, uint32_t vector, uint16_t flags)
{
    if (INTERRUPT_OVERRIDE_COUNT >= sizeof(INTERRUPT_OVERRIDES) / sizeof(INTERRUPT_OVERRIDES[0]))
        return;

    INTERRUPT_OVERRIDES[INTERRUPT_OVERRIDE_COUNT].Source = source;
    INTERRUPT_OVERRIDES[INTERRUPT_OVERRIDE_COUNT].Vector = vector;
    INTERRUPT_OVERRIDES[INTERRUPT_OVERRIDE_COUNT].Flags = flags;
    INTERRUPT_OVERRIDE_COUNT++;
}

void SetHardwareVector(uint8_t irq, uint8_t vector)
{
    // 默认认为全局系统中断号 (GSI) 顺次等于源 ISA 中断号
    uint32_t gsi = irq;
    uint16_t flags = 0;

    // 检查 ACPI MADT 账本中是否有特殊的硬件连线覆盖规则（比如经典的定时器 IRQ 0 经常被重映射到 GSI 2 上）
    for (uint32_t index = 0; index < INTERRUPT_OVERRIDE_COUNT; index++)
    {
        if (INTERRUPT_OVERRIDES[index].Source == irq)
        {
            gsi = INTERRUPT_OVERRIDES[index].Vector;
            flags = INTERRUPT_OVERRIDES[index].Flags;
            break;
        }
    }

    // 根据计算出的 GSI 路由去寻找对应的 IO APIC 芯片
    IO_APIC *ioApic = FindIOAPIC(gsi);
    if (ioApic == NULL)
        return;

    uint32_t pin = gsi - ioApic->VectorBase;
    uint32_t low = vector; // 填入低 32 位：也就是传入的、真正希望 CPU 响应的 IDT 向量号

    // 顺便解析 ACPI 记录的电气特性：配置该中断引脚是高电平触发还是低电平、边沿触发还是电平触发
    if ((flags & 0x3) == 0x3)
        low |= IO_APIC_REDIRECTION_ACTIVE_LOW;
    if (((flags >> 2) & 0x3) == 0x3)
        low |= IO_APIC_REDIRECTION_LEVEL_TRIGGERED;

    // 拼装高 32 位：告诉 IO APIC 当该引脚触发时，把中断精准投递到哪个 Local APIC 核心上
    uint32_t high = (ApicGetLocalId() & 0xFF) << 24;

    // 每一个重定向引脚由连续的两个 32 位寄存器控制，分别写入高低 32 位配置
    WriteIOAPIC(ioApic, IO_APIC_REDIRECTION_BASE + pin * 2 + 1, high);
    WriteIOAPIC(ioApic, IO_APIC_REDIRECTION_BASE + pin * 2, low);
}

static IO_APIC *FindIOAPIC(uint32_t global_system_interrupt)
{
    for (uint32_t index = 0; index < IO_APIC_COUNT; index++)
    {
        IO_APIC *ioApic = &IO_APICS[index];
        uint32_t end = ioApic->VectorBase + ioApic->RedirectionCount;

        if (global_system_interrupt >= ioApic->VectorBase && global_system_interrupt < end)
            return ioApic;
    }
    return NULL;
}

static uint32_t ReadIOAPIC(IO_APIC *io_apic, uint32_t register_index)
{
    io_apic->Address[IO_APIC_REGISTER_SELECT / sizeof(uint32_t)] = register_index;
    return io_apic->Address[IO_APIC_REGISTER_WINDOW / sizeof(uint32_t)];
}

static void WriteIOAPIC(IO_APIC *io_apic, uint32_t register_index, uint32_t value)
{
    io_apic->Address[IO_APIC_REGISTER_SELECT / sizeof(uint32_t)] = register_index;
    io_apic->Address[IO_APIC_REGISTER_WINDOW / sizeof(uint32_t)] = value;
}