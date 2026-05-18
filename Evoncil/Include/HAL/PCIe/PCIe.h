/** HAL/PCIe/PCIe.h
 *
 * (C) 2026 Charity Enol
 *
 * PCIe 设备和配置空间访问的 HAL 抽象。
 */

#ifndef HAL_PCIE_PCIE_H
#define HAL_PCIE_PCIE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * PCIe 设备信息。
 * 记录配置空间身份和已经解析出来的 BAR 物理地址。
 */
typedef struct
{
    uint8_t Bus;
    uint8_t Device;
    uint8_t Function;
    uint16_t VendorId;
    uint16_t DeviceId;
    uint8_t BaseClass;
    uint8_t SubClass;
    uint8_t ProgIF;
    uint8_t RevisionID;
    uint8_t HeaderType;
    uint64_t BAR[6];
} PCIeDevice;

/**
 * 寻找符合 Class 规范的第 index 个设备。
 * index 从 0 开始，用来枚举多个同类设备。
 */
bool PCIeFindDeviceByClass(
    uint8_t base_class,
    uint8_t sub_class,
    uint8_t prog_if,
    uint8_t index,
    PCIeDevice *device_out);

/**
 * 开启设备的 Memory Space 和 Bus Master 能力。
 * xHCI 这类 DMA 设备必须打开 Bus Master 才能访问内存中的 Ring。
 */
bool PCIeEnableBusMastering(const PCIeDevice *device);

/**
 * 读取 PCIe 配置空间中的 32 位字段。
 * 主要给 MSI/MSI-X 配置、BAR 解析和调试命令使用。
 */
bool PCIeReadConfig32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t *value);

/**
 * 写入 PCIe 配置空间中的 32 位字段。
 * 主要用于打开 Command 位和配置 MSI/MSI-X。
 */
bool PCIeWriteConfig32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t value);

#endif // HAL_PCIE_PCIE_H
