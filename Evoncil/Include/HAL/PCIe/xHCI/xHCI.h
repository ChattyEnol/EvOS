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

typedef struct
{
    PCIeDevice PciDevice; // 绑定的 PCIe 身份。
    void *MmioBase;       // 映射后的虚拟 MMIO 基地址。
    uint8_t MaxSlots;     // 支持的最大设备插槽。
    uint8_t MaxPorts;     // 芯片物理端口数。
} XHCI_CONTROLLER;

/**
 * 一键点亮 xHCI USB 控制器！
 * 内核启动时调它一次即可。它内部会干完所有脏活：
 * 1. 调用 PCIeFindDeviceByClass 揪出硬件。
 * 2. 把物理 BAR0 映射到 MmioBase 虚拟地址。
 * 3. 复变硬件、分配 xHCI 专用的命令/事件环形缓冲区（Ring）。
 * 4. 配置 MSI 中断，并让芯片开始对外轮询。
 *
 * @return 初始化并成功运行返回 true，硬件故障返回 false。
 */
bool InitXhci(void);

/**
 * xHCI 的核心中断服务例程（ISR）。
 * 当 USB 设备活动触发 MSI 中断后，x64/IDT 的 xHCI 向量处理函数
 * 会直接把控制权转交给这个函数。它负责去读 Event Ring。
 */
void XhciInterruptHandler(void);

/**
 * 查询 xHCI 控制器是否已经初始化并启动。
 */
bool IsXhciReady(void);

/**
 * 获取当前绑定的 xHCI 控制器信息。
 */
const XHCI_CONTROLLER *GetXhciController(void);

#endif // HAL_PCIE_XHCI_XHCI_H
