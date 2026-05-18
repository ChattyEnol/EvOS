/** HAL/X64/PCIe.c
 *
 * (C) 2026 Charity Enol
 *
 * x64 平台的 PCIe 配置空间访问与设备发现。
 */

#include <HAL/PCIe/PCIe.h>
#include <stddef.h>

#define PCI_CONFIG_ADDRESS_PORT 0xCF8
#define PCI_CONFIG_DATA_PORT 0xCFC

/**
 * 从 CPU 的指定 I/O 端口读取 32 位（4字节）数据。
 * 因为传统的配置空间访问必须走 CPU 的 IN/OUT 汇编指令，
 * 这个函数就是把这行底层汇编包了一层。
 * 
 * @note 服务对象：`ReadConfig32`。
 */
static inline uint32_t ReadPort32(uint16_t port);

/**
 * 向 CPU 的指定 I/O 端口写入 32 位（4字节）数据。
 * 把想要查询的设备地址和配置数据强行塞给主板。
 * 
 * @note 服务对象：WriteConfig32。
 */
static inline void WritePort32(uint16_t port, uint32_t value);

/**
 * 把设备的物理坐标打包成主板认识的 32 位专用 PCIe 地址。
 * 硬件只认固定的格式。这个函数把设备的 Bus（总线）、Device（设备）、
 * Function（功能）和寄存器偏移量（Offset）按照 PCIe 规范拼成一个 32 位的
 * 整数，最高位雷打不动地填 1（代表：老子现在要访问配置空间，别搞错了）。
 * 
 * @note 服务对象：`ReadConfig32` 和 `WriteConfig32`。
 */
static inline uint32_t BuildConfigAddress(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset);

/**
 * 读取任意设备配置空间里指定位置的 32 位数据。
 * 它的工作流程很经典：先用 `BuildConfigAddress` 拼好暗号，用 `WritePort32`
 * 把暗号丢给端口 `0xCF8`（告诉主板我要看谁），然后用 `ReadPort32` 从端口 `0xCFC`
 * 把主板吐出来的 4 字节数据接住。
 * 
 * @note 服务对象：`HasDevice`、`ReadDeviceHeader` 和整个模块的扫描函数。
 */
static bool ReadConfig32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t *value);

/**
 * 往任意设备配置空间里的指定位置写入 32 位数据。
 * 跟上面的读取很像，但它是用来改硬件参数的。比如后面开启外设的 DMA 能力、
 * 配置 MSI 中断映射时，全靠它把新参数真正写进硬件芯片里。
 * 
 * @note  服务对象：`PCIeEnableBusMastering`（对外公开接口）。
 */
static bool WriteConfig32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t value);

/**
 * 看看这个物理坐标上到底有没有插着设备。
 * 它的逻辑极其粗暴，直接去读该坐标设备的 0x00 寄存器（VendorID）。如果
 * 返回的是 0xFFFF，说明这块插槽是空的，连个鬼影都没有，后面就不用白费劲了。
 * 
 * @note  服务对象：`ReadDeviceHeader`。
 */
static bool HasDevice(uint8_t bus, uint8_t device, uint8_t function);

/**
 * 如果设备存在，负责把它的所有核心身份信息和 BAR 物理基地址全捞出来。
 * 这个函数是内部干脏活的集大成者。它先叫 `HasDevice` 确认有人，然后疯狂调用
 * `ReadConfig32` 把厂商 ID、设备 ID、分类代码（Class/SubClass）读出来。最关键的是，
 * 它会一口气扫描 6 个 BAR 寄存器，如果发现是真机上的 64 位大基址，它还会聪明地
 * 把两个 32 位寄存器拼成一个完整的 64 位虚拟/物理内存地址，打包填进外部要的结构体里。
 *
 * @note  服务对象：`PCIeFindDeviceByClass`（终极对外接口）。
 */
static bool ReadDeviceHeader(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    PCIeDevice *device_out);

/**
 * 头文件里的两个对外接口实现。
 */

bool PCIeEnableBusMastering(const PCIeDevice *device)
{
    if (device == NULL)
        return false;

    uint32_t data;
    if (!ReadConfig32(device->Bus, device->Device, device->Function, 0x04, &data))
        return false;

    data |= (1u << 2) | (1u << 1); // 开启 Bus Master 和 Memory Space
    return WriteConfig32(device->Bus, device->Device, device->Function, 0x04, data);
}

bool PCIeReadConfig32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t *value)
{
    return ReadConfig32(bus, device, function, offset, value);
}

bool PCIeWriteConfig32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t value)
{
    return WriteConfig32(bus, device, function, offset, value);
}

bool PCIeFindDeviceByClass(
    uint8_t base_class,
    uint8_t sub_class,
    uint8_t prog_if,
    uint8_t index,
    PCIeDevice *device_out)
{
    if (device_out == NULL)
        return false;

    uint8_t current_match = 0;

    for (uint16_t bus = 0; bus <= 0xFF; ++bus)
    {
        for (uint8_t device = 0; device < 32u; ++device)
        {
            uint32_t header0;
            if (!ReadConfig32((uint8_t)bus, device, 0, 0x00, &header0))
                continue;

            if ((uint16_t)header0 == 0xFFFFu)
                continue;

            uint8_t headerType = 0;
            if (!ReadConfig32((uint8_t)bus, device, 0, 0x0C, &header0))
                continue;

            headerType = (uint8_t)((header0 >> 16) & 0xFFu);
            uint8_t functionCount = (headerType & 0x80u) ? 8u : 1u;

            for (uint8_t function = 0; function < functionCount; ++function)
            {
                PCIeDevice candidate;
                if (!ReadDeviceHeader((uint8_t)bus, device, function, &candidate))
                    continue;

                if (candidate.BaseClass == base_class &&
                    candidate.SubClass == sub_class &&
                    candidate.ProgIF == prog_if)
                {
                    if (current_match == index)
                    {
                        *device_out = candidate;
                        return true;
                    }
                    current_match++;
                }
            }
        }
    }

    return false;
}

/**
 * 下面是上面用到的内部函数实现。
 */

static inline uint32_t ReadPort32(uint16_t port)
{
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void WritePort32(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0, %1" ::"a"(value), "Nd"(port));
}

static inline uint32_t BuildConfigAddress(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset)
{
    return (uint32_t)0x80000000u |
           ((uint32_t)bus << 16) |
           ((uint32_t)device << 11) |
           ((uint32_t)function << 8) |
           ((uint32_t)(offset & 0xFC));
}

static bool ReadConfig32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t *value)
{
    if (value == NULL || (offset & 0x3u) != 0)
        return false;

    WritePort32(PCI_CONFIG_ADDRESS_PORT, BuildConfigAddress(bus, device, function, offset));
    *value = ReadPort32((uint16_t)(PCI_CONFIG_DATA_PORT + (offset & 0x3u)));
    return true;
}

static bool WriteConfig32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t value)
{
    if ((offset & 0x3u) != 0)
        return false;

    WritePort32(PCI_CONFIG_ADDRESS_PORT, BuildConfigAddress(bus, device, function, offset));
    WritePort32((uint16_t)(PCI_CONFIG_DATA_PORT + (offset & 0x3u)), value);
    return true;
}

static bool HasDevice(uint8_t bus, uint8_t device, uint8_t function)
{
    uint32_t data;
    if (!ReadConfig32(bus, device, function, 0x00, &data))
        return false;

    return (uint16_t)data != 0xFFFFu;
}

static bool ReadDeviceHeader(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    PCIeDevice *device_out)
{
    if (device_out == NULL || !HasDevice(bus, device, function))
        return false;

    uint32_t data0;
    if (!ReadConfig32(bus, device, function, 0x00, &data0))
        return false;

    device_out->VendorId = (uint16_t)(data0 & 0xFFFFu);
    device_out->DeviceId = (uint16_t)((data0 >> 16) & 0xFFFFu);
    device_out->Bus = bus;
    device_out->Device = device;
    device_out->Function = function;

    uint32_t data4;
    if (!ReadConfig32(bus, device, function, 0x08, &data4))
        return false;

    device_out->RevisionID = (uint8_t)(data4 & 0xFFu);
    device_out->ProgIF = (uint8_t)((data4 >> 8) & 0xFFu);
    device_out->SubClass = (uint8_t)((data4 >> 16) & 0xFFu);
    device_out->BaseClass = (uint8_t)((data4 >> 24) & 0xFFu);

    uint32_t headerTypeData;
    if (!ReadConfig32(bus, device, function, 0x0C, &headerTypeData))
        return false;

    device_out->HeaderType = (uint8_t)((headerTypeData >> 16) & 0xFFu);

    for (uint8_t i = 0; i < 6; ++i)
    {
        uint8_t offset = 0x10 + i * 4;
        uint32_t barLow = 0;
        if (!ReadConfig32(bus, device, function, offset, &barLow))
            return false;

        // 判断是否是 MMIO
        if ((barLow & 0x1u) == 0u)
        {
            // 判断是否是 64-bit BAR
            if ((barLow & 0x6u) == 0x4u)
            {
                uint32_t barHigh = 0;
                if (!ReadConfig32(bus, device, function, offset + 4, &barHigh))
                    return false;

                uint64_t address = ((uint64_t)barHigh << 32) | (uint64_t)(barLow & ~0xFull);
                device_out->BAR[i] = address;
                i++; // 跳过下一个 32 位槽位
            }
            else
            {
                device_out->BAR[i] = (uint64_t)(barLow & ~0xFull);
            }
        }
        else
            // 传统的 I/O 端口 BAR
            device_out->BAR[i] = (uint64_t)(barLow & ~0x3u);
    }
    return true;
}
