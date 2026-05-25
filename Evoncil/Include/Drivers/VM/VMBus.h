/** Drivers/VM/VMBus.h
 *
 * (C) Charity Enol
 *
 * 虚拟机总线（VMBus）驱动核心接口。
 */

#ifndef DRIVERS_VM_VMBUS_H
#define DRIVERS_VM_VMBUS_H

#include <stdint.h>
#include <stdbool.h>

#define VMBUS_MAX_CHANNELS 256
#define VMBUS_INTERRUPT_VECTOR 80 // 给 VMBus 分配一个专属的 IDT 中断向量号

typedef struct
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} __attribute__((packed)) VMBUS_GUID;

// VMBus 环形缓冲区结构（由宿主机和虚拟机共享）
typedef struct
{
    volatile uint32_t WriteIndex;       // 写指针，单位是 Buffer 内的字节偏移。
    volatile uint32_t ReadIndex;        // 读指针，单位是 Buffer 内的字节偏移。
    volatile uint32_t InterruptMask;    // 是否屏蔽对方的中断通知。
    volatile uint32_t PendingSendSize;  // Hyper-V 用于流控的待发送大小。
    uint32_t Reserved1[12];
    uint32_t FeatureBits;
    uint8_t Reserved2[4096 - 68];       // 共享头固定占一页。
    uint8_t Buffer[];                   // 实际的数据缓冲区。
} __attribute__((packed)) VMBUS_RING_BUFFER;

// VMBus 通道描述符
typedef struct
{
    uint32_t ChannelId;                 // 宿主机分配的全局通道 ID
    uint16_t DeviceType;                // 设备类型（网卡、存储、鼠标等）
    VMBUS_GUID DeviceGuid;              // 宿主机 Offer 里的设备类型 GUID
    uint32_t ConnectionId;              // SignalEvent 用的连接 ID
    uint32_t GpadlHandle;               // Ring Buffer 对应的 GPADL 句柄
    uint32_t RingBufferSize;            // 单个 Ring Buffer 的总大小
    bool IsOpen;                        // 当前通道是否处于激活状态
    bool Offered;                       // 宿主机是否已经提供该通道
    VMBUS_RING_BUFFER *InboundBuffer;   // 接收缓冲区（宿主机写，内核读）
    VMBUS_RING_BUFFER *OutboundBuffer;  // 发送缓冲区（内核写，宿主机读）
    void (*Callback)(void *data);       // 收到该通道消息时的驱动回调
} VMBUS_CHANNEL;

/* 暴露给内核驱动层其他设备的公共接口 */

void InitVMBus(void);
bool VMBusOpenChannel(uint32_t channel_id, uint32_t buffer_size, void (*callback)(void *));
void VMBusWriteChannel(uint32_t channel_id, const void *buffer, uint32_t size);
bool VMBusFindChannelByGuid(const VMBUS_GUID *guid, uint32_t *channel_id);
bool VMBusSendPacket(
    uint32_t channel_id,
    const void *buffer,
    uint32_t size,
    uint64_t request_id,
    uint16_t packet_type,
    uint16_t flags);
bool VMBusReadPacket(
    uint32_t channel_id,
    void *buffer,
    uint32_t buffer_size,
    uint32_t *bytes_read,
    uint64_t *request_id);

#endif
