/** HAL/X64/xHCI.c
 *
 * (C) 2026 Charity Enol
 *
 * x64 平台的 xHCI 主控制器初始化与中断接入。
 */

#include <Drivers/Keyboard.h>
#include <HAL/HAL.h>
#include <HAL/PCIe/PCIe.h>
#include <HAL/PCIe/xHCI/xHCI.h>
#include <Noyau/Memory.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define XHCI_CAP_CAPLENGTH 0x00
#define XHCI_CAP_HCSPARAMS1 0x04
#define XHCI_CAP_HCCPARAMS1 0x10
#define XHCI_CAP_DBOFF 0x14
#define XHCI_CAP_RTSOFF 0x18

#define XHCI_OP_USBCMD 0x00
#define XHCI_OP_USBSTS 0x04
#define XHCI_OP_PAGESIZE 0x08
#define XHCI_OP_CRCR 0x18
#define XHCI_OP_DCBAAP 0x30
#define XHCI_OP_CONFIG 0x38

#define XHCI_RUN_IMAN 0x20
#define XHCI_RUN_IMOD 0x24
#define XHCI_RUN_ERSTSZ 0x28
#define XHCI_RUN_ERSTBA 0x30
#define XHCI_RUN_ERDP 0x38

#define XHCI_CMD_RS (1u << 0)
#define XHCI_CMD_HCRST (1u << 1)
#define XHCI_CMD_INTE (1u << 2)
#define XHCI_STS_HCH (1u << 0)
#define XHCI_STS_EINT (1u << 3)
#define XHCI_STS_CNR (1u << 11)
#define XHCI_IMAN_IP (1u << 0)
#define XHCI_IMAN_IE (1u << 1)
#define XHCI_ERDP_EHB (1ull << 3)

#define XHCI_TRB_CYCLE (1u << 0)
#define XHCI_TRB_TYPE_SHIFT 10
#define XHCI_TRB_TYPE_MASK 0x3Fu
#define XHCI_TRB_TYPE_TRANSFER_EVENT 32u
#define XHCI_TRB_TYPE_COMMAND_COMPLETION_EVENT 33u
#define XHCI_TRB_TYPE_PORT_STATUS_CHANGE_EVENT 34u

#define XHCI_INTERRUPT_VECTOR 34
#define XHCI_MMIO_SIZE 0x10000ull
#define XHCI_PAGE_SIZE 0x1000ull
#define XHCI_EVENT_RING_TRB_COUNT 256
#define XHCI_COMMAND_RING_TRB_COUNT 256

typedef struct
{
    uint64_t Parameter;
    uint32_t Status;
    uint32_t Control;
} __attribute__((packed, aligned(16))) XHCI_TRB;

typedef struct
{
    uint64_t RingSegmentBase;
    uint32_t RingSegmentSize;
    uint32_t Reserved;
} __attribute__((packed, aligned(16))) XHCI_EVENT_RING_SEGMENT;

static XHCI_CONTROLLER CONTROLLER = {0};
static uint32_t OpRegisterOffset = 0;
static uint32_t RuntimeRegisterOffset = 0;
static uint32_t DoorbellRegisterOffset = 0;
static bool XhciReady = false;

static XHCI_TRB *CommandRing = NULL;
static XHCI_TRB *EventRing = NULL;
static XHCI_EVENT_RING_SEGMENT *EventRingSegmentTable = NULL;
static void *DeviceContextBaseArray = NULL;
static uint64_t EventRingPhysical = 0;
static uint32_t EventRingIndex = 0;
static bool EventRingCycle = true;

/**
 * 读取 xHCI Capability Register 空间里的 32 位寄存器。
 */
static uint32_t ReadCap32(uint32_t offset);

/**
 * 读取 xHCI Operational Register 空间里的 32 位寄存器。
 */
static uint32_t ReadOp32(uint32_t offset);

/**
 * 写入 xHCI Operational Register 空间里的 32 位寄存器。
 */
static void WriteOp32(uint32_t offset, uint32_t value);

/**
 * 写入 xHCI Operational Register 空间里的 64 位寄存器。
 */
static void WriteOp64(uint32_t offset, uint64_t value);

/**
 * 读取 xHCI Runtime Register 空间里的 32 位寄存器。
 */
static uint32_t ReadRun32(uint32_t offset);

/**
 * 写入 xHCI Runtime Register 空间里的 32 位寄存器。
 */
static void WriteRun32(uint32_t offset, uint32_t value);

/**
 * 写入 xHCI Runtime Register 空间里的 64 位寄存器。
 */
static void WriteRun64(uint32_t offset, uint64_t value);

/**
 * 停止并复位 xHCI 控制器。
 */
static bool ResetController(void);

/**
 * 启动 xHCI 控制器，让它开始执行命令和投递事件。
 */
static bool StartController(void);

/**
 * 为 xHCI 准备命令环、事件环、ERST 和 DCBAA。
 */
static bool SetupXhciRings(void);

/**
 * 配置 Runtime Interrupter 0，使 MSI 进来的时候 Event Ring 可被消费。
 */
static bool SetupPrimaryInterrupter(void);

/**
 * 在 PCIe MSI 或 MSI-X Capability 里写入指定中断向量。
 */
static bool SetupXhciMsi(uint8_t vector);

/**
 * 写入单个 MSI Capability 的 Message Address、Message Data 和 Enable 位。
 */
static bool WriteMsiCapability(uint8_t cap_ptr, uint8_t vector);

/**
 * 写入单个 MSI-X Capability 的第 0 个 Table Entry。
 */
static bool WriteMsiXCapability(uint8_t cap_ptr, uint8_t vector);

/**
 * 消费 Event Ring 中所有已经由硬件投递的 TRB。
 */
static void ProcessEventRing(void);

/**
 * 处理单个事件 TRB。
 */
static void ProcessEventTRB(const XHCI_TRB *event_trb);

/**
 * 处理 Transfer Event，后续 HID Endpoint 完成时会从这里上报键盘报告。
 */
static void ProcessTransferEvent(const XHCI_TRB *event_trb);

/**
 * 把事件环消费位置写回 ERDP，并清理 Interrupter Pending 位。
 */
static void AcknowledgeInterrupter(void);

bool InitXhci(void)
{
    XhciReady = false;
    memset(&CONTROLLER, 0, sizeof(CONTROLLER));

    if (!PCIeFindDeviceByClass(0x0Cu, 0x03u, 0x30u, 0, &CONTROLLER.PciDevice))
        return false;

    if (!PCIeEnableBusMastering(&CONTROLLER.PciDevice))
        return false;

    uint64_t bar0Physical = CONTROLLER.PciDevice.BAR[0];
    if (bar0Physical == 0)
        return false;

    CONTROLLER.MmioBase = MapDeviceMemory(bar0Physical, XHCI_MMIO_SIZE);
    if (CONTROLLER.MmioBase == NULL)
        return false;

    OpRegisterOffset = *(volatile uint8_t *)((uintptr_t)CONTROLLER.MmioBase + XHCI_CAP_CAPLENGTH);
    DoorbellRegisterOffset = ReadCap32(XHCI_CAP_DBOFF) & ~0x3u;
    RuntimeRegisterOffset = ReadCap32(XHCI_CAP_RTSOFF) & ~0x1Fu;

    uint32_t params1 = ReadCap32(XHCI_CAP_HCSPARAMS1);
    CONTROLLER.MaxSlots = (uint8_t)(params1 & 0xFFu);
    CONTROLLER.MaxPorts = (uint8_t)((params1 >> 24) & 0xFFu);

    for (uint32_t retry = 0; retry < 100000u; retry++)
        if ((ReadOp32(XHCI_OP_USBSTS) & XHCI_STS_CNR) == 0)
            break;

    if ((ReadOp32(XHCI_OP_USBSTS) & XHCI_STS_CNR) != 0)
        return false;

    if (!ResetController())
        return false;

    if (!SetupXhciRings())
        return false;

    SetInterruptGate(XHCI_INTERRUPT_VECTOR, XhciInterruptHandler);
    if (!SetupXhciMsi(XHCI_INTERRUPT_VECTOR))
        return false;

    if (!StartController())
        return false;

    XhciReady = true;
    return true;
}

void XhciInterruptHandler(void)
{
    uint32_t status = ReadOp32(XHCI_OP_USBSTS);

    if ((status & XHCI_STS_EINT) != 0)
    {
        ProcessEventRing();
        WriteOp32(XHCI_OP_USBSTS, XHCI_STS_EINT);
        AcknowledgeInterrupter();
    }
}

bool IsXhciReady(void)
{
    return XhciReady;
}

const XHCI_CONTROLLER *GetXhciController(void)
{
    return &CONTROLLER;
}

static uint32_t ReadCap32(uint32_t offset)
{
    volatile uint32_t *address =
        (volatile uint32_t *)((uintptr_t)CONTROLLER.MmioBase + offset);

    return *address;
}

static uint32_t ReadOp32(uint32_t offset)
{
    volatile uint32_t *address =
        (volatile uint32_t *)((uintptr_t)CONTROLLER.MmioBase + OpRegisterOffset + offset);

    return *address;
}

static void WriteOp32(uint32_t offset, uint32_t value)
{
    volatile uint32_t *address =
        (volatile uint32_t *)((uintptr_t)CONTROLLER.MmioBase + OpRegisterOffset + offset);

    *address = value;
}

static void WriteOp64(uint32_t offset, uint64_t value)
{
    WriteOp32(offset, (uint32_t)(value & 0xFFFFFFFFu));
    WriteOp32(offset + 4, (uint32_t)(value >> 32));
}

static uint32_t ReadRun32(uint32_t offset)
{
    volatile uint32_t *address =
        (volatile uint32_t *)((uintptr_t)CONTROLLER.MmioBase + RuntimeRegisterOffset + offset);

    return *address;
}

static void WriteRun32(uint32_t offset, uint32_t value)
{
    volatile uint32_t *address =
        (volatile uint32_t *)((uintptr_t)CONTROLLER.MmioBase + RuntimeRegisterOffset + offset);

    *address = value;
}

static void WriteRun64(uint32_t offset, uint64_t value)
{
    WriteRun32(offset, (uint32_t)(value & 0xFFFFFFFFu));
    WriteRun32(offset + 4, (uint32_t)(value >> 32));
}

static bool ResetController(void)
{
    uint32_t command = ReadOp32(XHCI_OP_USBCMD);
    command &= ~XHCI_CMD_RS;
    WriteOp32(XHCI_OP_USBCMD, command);

    for (uint32_t retry = 0; retry < 100000u; retry++)
        if ((ReadOp32(XHCI_OP_USBSTS) & XHCI_STS_HCH) != 0)
            break;

    if ((ReadOp32(XHCI_OP_USBSTS) & XHCI_STS_HCH) == 0)
        return false;

    WriteOp32(XHCI_OP_USBCMD, XHCI_CMD_HCRST);

    for (uint32_t retry = 0; retry < 100000u; retry++)
        if ((ReadOp32(XHCI_OP_USBCMD) & XHCI_CMD_HCRST) == 0)
            return true;

    return false;
}

static bool StartController(void)
{
    uint32_t command = ReadOp32(XHCI_OP_USBCMD);
    command |= XHCI_CMD_RS | XHCI_CMD_INTE;
    WriteOp32(XHCI_OP_USBCMD, command);

    for (uint32_t retry = 0; retry < 100000u; retry++)
        if ((ReadOp32(XHCI_OP_USBSTS) & XHCI_STS_HCH) == 0)
            return true;

    return false;
}

static bool SetupXhciRings(void)
{
    CommandRing = AllocatePage(XHCI_PAGE_SIZE);
    EventRing = AllocatePage(XHCI_PAGE_SIZE);
    EventRingSegmentTable = AllocatePage(XHCI_PAGE_SIZE);
    DeviceContextBaseArray = AllocatePage(XHCI_PAGE_SIZE);

    if (CommandRing == NULL ||
        EventRing == NULL ||
        EventRingSegmentTable == NULL ||
        DeviceContextBaseArray == NULL)
        return false;

    memset(CommandRing, 0, XHCI_PAGE_SIZE);
    memset(EventRing, 0, XHCI_PAGE_SIZE);
    memset(EventRingSegmentTable, 0, XHCI_PAGE_SIZE);
    memset(DeviceContextBaseArray, 0, XHCI_PAGE_SIZE);

    uint64_t commandRingPhysical = GetPhysicalAddress(CommandRing);
    EventRingPhysical = GetPhysicalAddress(EventRing);
    uint64_t eventRingSegmentTablePhysical = GetPhysicalAddress(EventRingSegmentTable);
    uint64_t dcbaaPhysical = GetPhysicalAddress(DeviceContextBaseArray);

    if (commandRingPhysical == 0 ||
        EventRingPhysical == 0 ||
        eventRingSegmentTablePhysical == 0 ||
        dcbaaPhysical == 0)
        return false;

    EventRingSegmentTable[0].RingSegmentBase = EventRingPhysical;
    EventRingSegmentTable[0].RingSegmentSize = XHCI_EVENT_RING_TRB_COUNT;
    EventRingSegmentTable[0].Reserved = 0;

    WriteOp64(XHCI_OP_CRCR, commandRingPhysical | XHCI_TRB_CYCLE);
    WriteOp64(XHCI_OP_DCBAAP, dcbaaPhysical);
    WriteOp32(XHCI_OP_CONFIG, CONTROLLER.MaxSlots);

    EventRingIndex = 0;
    EventRingCycle = true;

    return SetupPrimaryInterrupter();
}

static bool SetupPrimaryInterrupter(void)
{
    if (RuntimeRegisterOffset == 0 || EventRingPhysical == 0 || EventRingSegmentTable == NULL)
        return false;

    uint64_t eventRingSegmentTablePhysical = GetPhysicalAddress(EventRingSegmentTable);
    if (eventRingSegmentTablePhysical == 0)
        return false;

    WriteRun32(XHCI_RUN_IMOD, 0);
    WriteRun32(XHCI_RUN_ERSTSZ, 1);
    WriteRun64(XHCI_RUN_ERSTBA, eventRingSegmentTablePhysical);
    WriteRun64(XHCI_RUN_ERDP, EventRingPhysical | XHCI_ERDP_EHB);
    WriteRun32(XHCI_RUN_IMAN, XHCI_IMAN_IE);
    return true;
}

static bool SetupXhciMsi(uint8_t vector)
{
    uint8_t bus = CONTROLLER.PciDevice.Bus;
    uint8_t device = CONTROLLER.PciDevice.Device;
    uint8_t function = CONTROLLER.PciDevice.Function;
    uint32_t statusRegister;

    if (!PCIeReadConfig32(bus, device, function, 0x04, &statusRegister))
        return false;

    if ((statusRegister & (1u << 20)) == 0)
        return false;

    uint32_t capabilityRegister;
    if (!PCIeReadConfig32(bus, device, function, 0x34, &capabilityRegister))
        return false;

    uint8_t capPtr = (uint8_t)(capabilityRegister & 0xFCu);
    uint8_t msiXCapPtr = 0;

    while (capPtr != 0)
    {
        uint32_t capability;
        if (!PCIeReadConfig32(bus, device, function, capPtr & 0xFCu, &capability))
            return false;

        uint8_t capID = (uint8_t)(capability & 0xFFu);
        uint8_t nextCapPtr = (uint8_t)((capability >> 8) & 0xFCu);

        if (capID == 0x05 && WriteMsiCapability(capPtr, vector))
            return true;

        if (capID == 0x11)
            msiXCapPtr = capPtr;

        capPtr = nextCapPtr;
    }

    if (msiXCapPtr != 0)
        return WriteMsiXCapability(msiXCapPtr, vector);

    return false;
}

static bool WriteMsiCapability(uint8_t cap_ptr, uint8_t vector)
{
    uint8_t bus = CONTROLLER.PciDevice.Bus;
    uint8_t device = CONTROLLER.PciDevice.Device;
    uint8_t function = CONTROLLER.PciDevice.Function;
    uint32_t controlRegister;
    uint8_t controlOffset = cap_ptr + 2;
    uint8_t alignedControlOffset = controlOffset & 0xFCu;

    if (!PCIeReadConfig32(bus, device, function, alignedControlOffset, &controlRegister))
        return false;

    uint32_t controlShift = (controlOffset & 0x3u) * 8;
    uint16_t messageControl = (uint16_t)((controlRegister >> controlShift) & 0xFFFFu);
    bool has64BitAddress = (messageControl & (1u << 7)) != 0;

    if (!PCIeWriteConfig32(bus, device, function, cap_ptr + 4, GetMSIMessageAddress()))
        return false;

    uint8_t dataOffset = cap_ptr + 8;
    if (has64BitAddress)
    {
        if (!PCIeWriteConfig32(bus, device, function, cap_ptr + 8, 0))
            return false;

        dataOffset = cap_ptr + 12;
    }

    if (!PCIeWriteConfig32(bus, device, function, dataOffset, GetMSIMessageData(vector)))
        return false;

    messageControl |= 1u;

    uint32_t mask = 0xFFFFu << controlShift;
    uint32_t newControlRegister =
        (controlRegister & ~mask) | ((uint32_t)messageControl << controlShift);

    return PCIeWriteConfig32(bus, device, function, alignedControlOffset, newControlRegister);
}

static bool WriteMsiXCapability(uint8_t cap_ptr, uint8_t vector)
{
    uint8_t bus = CONTROLLER.PciDevice.Bus;
    uint8_t device = CONTROLLER.PciDevice.Device;
    uint8_t function = CONTROLLER.PciDevice.Function;
    uint32_t tableRegister;
    uint32_t controlRegister;

    if (!PCIeReadConfig32(bus, device, function, cap_ptr + 4, &tableRegister))
        return false;

    if (!PCIeReadConfig32(bus, device, function, cap_ptr & 0xFCu, &controlRegister))
        return false;

    uint8_t tableBar = (uint8_t)(tableRegister & 0x7u);
    uint32_t tableOffset = tableRegister & ~0x7u;
    if (tableBar >= 6 || CONTROLLER.PciDevice.BAR[tableBar] == 0)
        return false;

    volatile uint32_t *table = (volatile uint32_t *)MapDeviceMemory(
        CONTROLLER.PciDevice.BAR[tableBar] + tableOffset,
        XHCI_PAGE_SIZE);
    if (table == NULL)
        return false;

    table[3] = 1u;
    table[0] = GetMSIMessageAddress();
    table[1] = 0;
    table[2] = GetMSIMessageData(vector);
    table[3] = 0;

    uint16_t messageControl = (uint16_t)((controlRegister >> 16) & 0xFFFFu);
    messageControl |= (1u << 15);
    messageControl &= (uint16_t)~(1u << 14);

    uint32_t newControlRegister =
        (controlRegister & 0x0000FFFFu) | ((uint32_t)messageControl << 16);

    return PCIeWriteConfig32(bus, device, function, cap_ptr & 0xFCu, newControlRegister);
}

static void ProcessEventRing(void)
{
    if (EventRing == NULL)
        return;

    for (uint32_t handled = 0; handled < XHCI_EVENT_RING_TRB_COUNT; handled++)
    {
        XHCI_TRB *eventTrb = &EventRing[EventRingIndex];
        bool cycle = (eventTrb->Control & XHCI_TRB_CYCLE) != 0;
        if (cycle != EventRingCycle)
            break;

        ProcessEventTRB(eventTrb);

        EventRingIndex++;
        if (EventRingIndex >= XHCI_EVENT_RING_TRB_COUNT)
        {
            EventRingIndex = 0;
            EventRingCycle = !EventRingCycle;
        }
    }
}

static void ProcessEventTRB(const XHCI_TRB *event_trb)
{
    uint32_t type = (event_trb->Control >> XHCI_TRB_TYPE_SHIFT) & XHCI_TRB_TYPE_MASK;

    switch (type)
    {
    case XHCI_TRB_TYPE_TRANSFER_EVENT:
        ProcessTransferEvent(event_trb);
        break;
    case XHCI_TRB_TYPE_COMMAND_COMPLETION_EVENT:
    case XHCI_TRB_TYPE_PORT_STATUS_CHANGE_EVENT:
    default:
        break;
    }
}

static void ProcessTransferEvent(const XHCI_TRB *event_trb)
{
    (void)event_trb;

    /**
     * HID Endpoint 枚举与 Interrupt IN Transfer Ring 接上后，
     * 在这里读取完成的 8 字节 Boot Keyboard Report：
     *
     * SubmitKeyboardHIDReport(report, 8);
     *
     * 这样 xHCI 层只负责 USB 传输完成，键盘层只负责 HID -> KEY_CODE。
     */
}

static void AcknowledgeInterrupter(void)
{
    if (EventRingPhysical == 0)
        return;

    uint64_t dequeuePointer =
        EventRingPhysical + (uint64_t)EventRingIndex * sizeof(XHCI_TRB);

    WriteRun64(XHCI_RUN_ERDP, dequeuePointer | XHCI_ERDP_EHB);
    WriteRun32(XHCI_RUN_IMAN, ReadRun32(XHCI_RUN_IMAN) | XHCI_IMAN_IP | XHCI_IMAN_IE);
}
