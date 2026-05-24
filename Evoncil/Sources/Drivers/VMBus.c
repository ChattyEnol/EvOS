/** Drivers/VMBus.c
 *
 * (C) Charity Enol
 *
 * VMBus 总线控制与内存管道实现。
 */

#include <EvOS.h>
#include <HAL/HAL.h>
#include <Drivers/VM/VMBus.h>
#include <Noyau/Memory.h>

#include <stddef.h>

// Hyper-V 投递消息的通用包裹结构
typedef struct
{
    uint32_t ConnectionId;
    uint32_t Reserved;
    uint32_t MessageType;
    uint32_t PayloadSize;
    uint8_t Payload[240];
} __attribute__((packed)) HV_POST_MESSAGE_INPUT;

// Hyper-V 接收消息的通用结构
typedef struct
{
    uint32_t MessageType;
    uint8_t PayloadSize;
    uint8_t MessageFlags;
    uint16_t Reserved;
    uint64_t SenderId;
    uint8_t Payload[240];
} __attribute__((packed)) HV_MESSAGE;

// 异步握手红绿灯标志
static volatile bool ContactResponseReceived = false;
static volatile bool GpadlCreatedReceived = false;
static volatile bool OpenChannelReceived = false;

// VMBus 核心协议消息类型常量
#define VMBUS_MSG_OPEN_CHANNEL 2
#define VMBUS_MSG_GPADL_HEADER 8
#define VMBUS_MSG_INITIATE_CONTACT 14

// 初始化总线连接
typedef struct
{
    uint32_t MessageType;
    uint32_t Reserved;
    uint64_t Version;
    uint64_t InterruptPage;
} __attribute__((packed)) VMBUS_CHANNEL_INITIATE_CONTACT;

// 向宿主机上报并注册物理内存通行证（GPADL）
typedef struct
{
    uint32_t MessageType;
    uint32_t ChannelId;
    uint32_t GpadlHandle;
    uint16_t RangeBufLen;
    uint16_t RangeCount;
    uint32_t ByteLength;
    uint32_t ByteOffset;
    uint64_t PfnArray[2]; // 咱们的接收和发送缓冲区刚好占用 2 个物理页
} __attribute__((packed)) VMBUS_CHANNEL_GPADL_HEADER;

// 正式向宿主机申请开启特定通道
typedef struct
{
    uint32_t MessageType;
    uint32_t ChannelId;
    uint32_t OpenId;
    uint32_t GpadlHandle;
    uint32_t DownstreamRingBufferPageOffset;
    uint32_t TargetVcpu;
    uint8_t UserData[120];
} __attribute__((packed)) VMBUS_CHANNEL_OPEN_CHANNEL;

static VMBUS_CHANNEL VMBusChannels[VMBUS_MAX_CHANNELS];
static void *VMBusMessagePage = NULL;
static void *VMBusEventPage = NULL;
static void *VMBusPostPage = NULL; // 专门用来通过 MSR 往宿主机灌协议数据的虚拟页面

static void InterruptHandler(void);
static void NotifyHost(uint32_t channel_id);
static bool PostMessage(const void *payload, uint32_t size);

void InitVMBus(void)
{
    // 清空通道账本
    for (uint32_t i = 0; i < VMBUS_MAX_CHANNELS; i++)
    {
        VMBusChannels[i].IsOpen = false;
        VMBusChannels[i].Callback = NULL;
    }

    // 分配宿主机与我们进行信令交互的物理页
    VMBusMessagePage = AllocatePage(4096);
    VMBusEventPage = AllocatePage(4096);
    VMBusPostPage = AllocatePage(4096);

    SetupHypervisor(GetPhysicalAddress(VMBusMessagePage), GetPhysicalAddress(VMBusEventPage));
    SetInterruptHandler(VMBUS_INTERRUPT_VECTOR, InterruptHandler);

    // 第一步：向宿主机发送 Initiate Contact 消息，宣告 EvOS 总线层正式上线
    VMBUS_CHANNEL_INITIATE_CONTACT contact;
    contact.MessageType = VMBUS_MSG_INITIATE_CONTACT;
    contact.Reserved = 0;
    contact.Version = 0x00030000; // 经典的 Windows 8 契约版本，兼容性极佳
    contact.InterruptPage = 0;    // 告诉宿主机直接使用 SynIC 的事件页做通知

    ContactResponseReceived = false;
    PostMessage(&contact, sizeof(contact));

    kprintf("VMBus: Initiate Contact sent, waiting for response...\n");

    volatile uint64_t *msg_page_ptr = (volatile uint64_t *)VMBusMessagePage;
    kprintf("--- Message Page Dump ---\n");
    for (int i = 0; i < 512; i++)
    {
        // 巧妙利用 %p 来读取并打印 64 位数据
        kprintf("%p ", (void *)msg_page_ptr[i]);

        // 每行打 8 个，省得屏幕滚得太快
        if ((i + 1) % 16 == 0)
            kprintf("\n");
    }

    // 阻塞等待宿主机的上线确认消息，直到中断处理程序把它拨绿
    while (!ContactResponseReceived)
        Pause();
    kprintf("VMBus: Contact response received, bus is online!\n");
}

bool VMBusOpenChannel(uint32_t channel_id, uint32_t buffer_size, void (*callback)(void *))
{
    if (channel_id >= VMBUS_MAX_CHANNELS || VMBusChannels[channel_id].IsOpen)
        return false;

    VMBUS_CHANNEL *chan = &VMBusChannels[channel_id];
    chan->ChannelId = channel_id;
    chan->Callback = callback;

    // 拒绝幽灵硬编码！动态向内存管理器申请真正合法的物理页面
    void *outbound_virt = AllocatePage(buffer_size);
    void *inbound_virt = AllocatePage(buffer_size);
    if (outbound_virt == NULL || inbound_virt == NULL)
        return false;

    // 获取这两个页面真实可靠的物理地址
    uint64_t outbound_phy = GetPhysicalAddress(outbound_virt);
    uint64_t inbound_phy = GetPhysicalAddress(inbound_virt);

    chan->OutboundBuffer = (VMBUS_RING_BUFFER *)outbound_virt;
    chan->InboundBuffer = (VMBUS_RING_BUFFER *)inbound_virt;

    chan->OutboundBuffer->WriteIndex = 0;
    chan->OutboundBuffer->ReadIndex = 0;
    chan->InboundBuffer->WriteIndex = 0;
    chan->InboundBuffer->ReadIndex = 0;

    // 第二步：将这两个物理页打包上报，向宿主机申请一个内存数字通行证（GPADL Handle）
    VMBUS_CHANNEL_GPADL_HEADER gpadl;
    gpadl.MessageType = VMBUS_MSG_GPADL_HEADER;
    gpadl.ChannelId = channel_id;
    gpadl.GpadlHandle = channel_id + 100; // 只要保证唯一，我们给它指定一个简单的标识
    gpadl.RangeBufLen = 24;               // 包裹头 8 字节 + 2 个 PFN 占 16 字节
    gpadl.RangeCount = 1;
    gpadl.ByteLength = buffer_size * 2; // 这块内存通行证总共涵盖了发送和接收两片区域
    gpadl.ByteOffset = 0;
    gpadl.PfnArray[0] = outbound_phy >> 12; // 算出真实的物理页帧号
    gpadl.PfnArray[1] = inbound_phy >> 12;

    GpadlCreatedReceived = false;
    PostMessage(&gpadl, sizeof(gpadl));

    // 阻塞等待宿主机把物理内存登记入册
    while (!GpadlCreatedReceived)
        Pause();

    // 第三步：手握刚刚批下来的通行证，正式向宿主机发出通道开通申请
    VMBUS_CHANNEL_OPEN_CHANNEL open_msg;
    open_msg.MessageType = VMBUS_MSG_OPEN_CHANNEL;
    open_msg.ChannelId = channel_id;
    open_msg.OpenId = channel_id;
    open_msg.GpadlHandle = channel_id + 100;
    open_msg.DownstreamRingBufferPageOffset = buffer_size / 4096; // 接收环在第一页之后（偏移为 1）
    open_msg.TargetVcpu = 0;                                      // 指定由 0 号核心处理
    for (uint32_t i = 0; i < 120; i++)
        open_msg.UserData[i] = 0;

    OpenChannelReceived = false;
    PostMessage(&open_msg, sizeof(open_msg));

    // 阻塞等待通道彻底大开的捷报
    while (!OpenChannelReceived)
        Pause();

    chan->IsOpen = true;
    return true;
}

void VMBusWriteChannel(uint32_t channel_id, const void *buffer, uint32_t size)
{
    VMBUS_CHANNEL *chan = &VMBusChannels[channel_id];
    if (!chan->IsOpen)
        return;

    VMBUS_RING_BUFFER *ring = chan->OutboundBuffer;
    uint32_t write_ptr = ring->WriteIndex;

    uint8_t *data_ptr = (uint8_t *)buffer;
    for (uint32_t i = 0; i < size; i++)
    {
        ring->Buffer[write_ptr] = data_ptr[i];
        write_ptr = (write_ptr + 1) % 4096;
    }

    ring->WriteIndex = write_ptr;

    if (!ring->InterruptMask)
        NotifyHost(channel_id);
}

/**
 * 核心中断服务程序：当宿主机往我们的 MessagePage 里塞入控制消息，
 * 或者往 EventPage 里戳了一下事件位时，CPU 就会直接弹进这个函数。
 */
static void InterruptHandler(void)
{
    // 1. 消息页精准肉搏：强转时直接偏移 2 个 SINT 房间（512 字节）
    volatile HV_MESSAGE *msg = (volatile HV_MESSAGE *)((uint8_t *)VMBusMessagePage + 256 * 2);

    if (msg->MessageType != 0)
    {
        // 根据人肉抓包数据，0xf 躲在 Payload 往后 8 字节的位置
        uint32_t vmbus_msg_type = *(volatile uint32_t *)(msg->Payload + 8);

        if (vmbus_msg_type == 15) // VMBUS_MSG_INITIATE_CONTACT_RESPONSE
            ContactResponseReceived = true;
        else if (vmbus_msg_type == 9) // VMBUS_MSG_GPADL_CREATED
            GpadlCreatedReceived = true;
        else if (vmbus_msg_type == 3) // VMBUS_MSG_OPEN_CHANNEL_RESULT
            OpenChannelReceived = true;

        // 读完必须清空消息类型，给这间属于 SINT 2 的房间解锁
        msg->MessageType = 0;

        AckHyperMessage();
    }

    // 2. 事件页同步纠偏：事件通知也必须切到 SINT 2 的专属领地
    volatile uint64_t *event_bits = (volatile uint64_t *)((uint8_t *)VMBusEventPage + 256 * 2);
    for (uint32_t i = 0; i < VMBUS_MAX_CHANNELS; i++)
    {
        if (event_bits[i / 64] & (1ull << (i % 64)))
        {
            event_bits[i / 64] &= ~(1ull << (i % 64));
            if (VMBusChannels[i].IsOpen && VMBusChannels[i].Callback != NULL)
                VMBusChannels[i].Callback(&VMBusChannels[i]);
        }
    }
}

/**
 * 通过通用的 Hypercall 接口，通知宿主机来收货。
 */
static void NotifyHost(uint32_t channel_id)
{
    uint64_t control_code = 0x005D;
    Hypercall(control_code, channel_id);
}

static bool PostMessage(const void *payload, uint32_t size)
{
    if (VMBusPostPage == NULL || size > 240)
        return false;

    volatile HV_POST_MESSAGE_INPUT *input = (volatile HV_POST_MESSAGE_INPUT *)VMBusPostPage;
    input->ConnectionId = 1; // VMBus 标准控制通道连接 ID 固定为 1
    input->MessageType = 1;  // 数据载荷类型
    input->PayloadSize = size;
    input->Reserved = 0;

    const uint8_t *src = (const uint8_t *)payload;
    for (uint32_t i = 0; i < size; i++)
        input->Payload[i] = src[i];

    for (uint32_t i = size; i < 240; i++)
        input->Payload[i] = 0;
    return Hypercall(0x005C, GetPhysicalAddress(VMBusPostPage)) == 0;
}