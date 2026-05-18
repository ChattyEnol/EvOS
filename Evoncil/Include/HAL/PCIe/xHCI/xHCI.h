/** HAL/PCIe/xHCI/xHCI.h
 *
 * (C) 2026 Charity Enol
 *
 * xHCI 主控制器的 HAL 层抽象接口。
 */

#ifndef HAL_PCIE_XHCI_XHCI_H
#define HAL_PCIE_XHCI_XHCI_H

#include <HAL/PCIe/PCIe.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * xHCI 控制器运行状态。
 * 保存 PCIe 身份、MMIO 映射地址和硬件能力参数。
 */
typedef struct
{
    PCIeDevice PciDevice; // 绑定的 PCIe 身份。
    void *MmioBase;       // 映射后的虚拟 MMIO 基地址。
    uint8_t MaxSlots;     // 支持的最大设备插槽。
    uint8_t MaxPorts;     // 芯片物理端口数。
} XHCI_CONTROLLER;

/**
 * 初始化第一个 xHCI USB 控制器。
 * 会完成 PCIe 发现、BAR0 MMIO 映射、控制器复位、MSI 注册和启动。
 */
bool InitXhci(void);

/**
 * xHCI 中断服务例程。
 * 由中断分发模块调用，用来确认并清除 xHCI 事件中断。
 */
void XhciInterruptHandler(void);

/**
 * 查询 xHCI 控制器是否已经成功初始化。
 */
bool IsXhciReady(void);

/**
 * 获取 xHCI 控制器状态结构。
 * 主要给控制台 info 和后续 USB 枚举代码读取硬件参数。
 */
const XHCI_CONTROLLER *GetXhciController(void);

#endif // HAL_PCIE_XHCI_XHCI_H
