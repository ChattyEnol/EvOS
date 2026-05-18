/** HAL/X64/xHCI.c
 *
 * (C) 2026 Charity Enol
 *
 * x64 平台的 xHCI 主控制器初始化与中断接入。
 */

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

#define XHCI_OP_USBCMD 0x00
#define XHCI_OP_USBSTS 0x04
#define XHCI_OP_CRCR 0x18

#define XHCI_CMD_RS (1u << 0)
#define XHCI_CMD_HCRST (1u << 1)
#define XHCI_STS_HCH (1u << 0)
#define XHCI_STS_EINT (1u << 3)
#define XHCI_STS_CNR (1u << 11)

#define XHCI_INTERRUPT_VECTOR 34
#define XHCI_MMIO_SIZE 0x10000ull
#define XHCI_PAGE_SIZE 0x1000ull

static XHCI_CONTROLLER CONTROLLER = {0};
static uint32_t OpRegisterOffset = 0;
static bool XhciReady = false;

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
 * 停止并复位 xHCI 控制器。
 */
static bool ResetController(void);

/**
 * 启动 xHCI 控制器，让它开始执行命令和投递事件。
 */
static bool StartController(void);

/**
 * 为 xHCI 准备最小命令环。
 */
static void SetupXhciRings(void);

/**
 * 在 PCIe MSI Capability 里写入指定中断向量。
 */
static bool SetupXhciMsi(uint8_t vector);

/**
 * 写入单个 MSI Capability 的 Message Address、Message Data 和 Enable 位。
 */
static bool WriteMsiCapability(uint8_t cap_ptr, uint8_t vector);

/**
 * 临时把虚拟地址当作物理地址使用。
 * 后续需要替换为页表查询或 DMA 专用分配器。
 */
static uint64_t GetPhysicalFromVirtual(void *virtual_address);

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

    SetupXhciRings();
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
        WriteOp32(XHCI_OP_USBSTS, XHCI_STS_EINT);

    /**
     * TODO:
     * 这里之后要解析 Event Ring，拿到 Transfer Event，再把 HID 报文翻译成
     * KEYBOARD_EVENT 塞给统一键盘输入队列。
     */
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
    command |= XHCI_CMD_RS;
    WriteOp32(XHCI_OP_USBCMD, command);

    for (uint32_t retry = 0; retry < 100000u; retry++)
        if ((ReadOp32(XHCI_OP_USBSTS) & XHCI_STS_HCH) == 0)
            return true;

    return false;
}

static void SetupXhciRings(void)
{
    void *commandRing = AllocatePage(XHCI_PAGE_SIZE);
    if (commandRing == NULL)
        return;

    memset(commandRing, 0, XHCI_PAGE_SIZE);

    uint64_t commandRingPhysical = GetPhysicalFromVirtual(commandRing);
    WriteOp32(XHCI_OP_CRCR, (uint32_t)(commandRingPhysical & 0xFFFFFFFFu));
    WriteOp32(XHCI_OP_CRCR + 4, (uint32_t)(commandRingPhysical >> 32));
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
    while (capPtr != 0)
    {
        uint32_t capability;
        if (!PCIeReadConfig32(bus, device, function, capPtr & 0xFCu, &capability))
            return false;

        uint8_t capID = (uint8_t)(capability & 0xFFu);
        uint8_t nextCapPtr = (uint8_t)((capability >> 8) & 0xFCu);

        if (capID == 0x05)
            return WriteMsiCapability(capPtr, vector);

        capPtr = nextCapPtr;
    }

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

static uint64_t GetPhysicalFromVirtual(void *virtual_address)
{
    return (uint64_t)(uintptr_t)virtual_address;
}
