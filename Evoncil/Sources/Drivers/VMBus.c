/** Drivers/VMBus.c
 *
 * (C) Charity Enol
 *
 * Hyper-V VMBus 总线控制与通道 Ring Buffer 实现。
 */

#include <Drivers/VM/VMBus.h>
#include <EvOS.h>
#include <HAL/HAL.h>
#include <Noyau/Memory.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define HV_MESSAGE_PAYLOAD_SIZE 240
#define HV_MESSAGE_SIZE 256
#define HV_MESSAGE_SINT 2
#define HV_STATUS_SUCCESS 0

#define HVCALL_POST_MESSAGE 0x005C
#define HVCALL_SIGNAL_EVENT 0x005D
#define HV_HYPERCALL_FAST_BIT (1ull << 16)

#define VMBUS_MESSAGE_CONNECTION_ID 1
#define VMBUS_MESSAGE_CONNECTION_ID_4 4
#define VMBUS_EVENT_CONNECTION_ID 2
#define VMBUS_VERSION_WIN10_V5 0x00050000u

#define VMBUS_MSG_OFFER_CHANNEL 1
#define VMBUS_MSG_REQUEST_OFFERS 3
#define VMBUS_MSG_ALL_OFFERS_DELIVERED 4
#define VMBUS_MSG_OPEN_CHANNEL 5
#define VMBUS_MSG_OPEN_CHANNEL_RESULT 6
#define VMBUS_MSG_GPADL_HEADER 8
#define VMBUS_MSG_GPADL_CREATED 10
#define VMBUS_MSG_INITIATE_CONTACT 14
#define VMBUS_MSG_VERSION_RESPONSE 15

#define VMBUS_PACKET_DATA_INBAND 0x6
#define VMBUS_PACKET_COMPLETION_REQUESTED 0x1

#define PAGE_SIZE 4096u
#define GPADL_HANDLE_BASE 0xE100u
#define GPADL_MAX_PFN_COUNT 32u
#define VMBUS_WAIT_TIMEOUT 100000000u

typedef struct
{
    uint32_t ConnectionId;
    uint32_t Reserved;
    uint32_t MessageType;
    uint32_t PayloadSize;
    uint8_t Payload[HV_MESSAGE_PAYLOAD_SIZE];
} __attribute__((packed)) HV_POST_MESSAGE_INPUT;

typedef struct
{
    uint32_t MessageType;
    uint8_t PayloadSize;
    uint8_t MessageFlags;
    uint16_t Reserved;
    uint64_t SenderId;
    uint8_t Payload[HV_MESSAGE_PAYLOAD_SIZE];
} __attribute__((packed)) HV_MESSAGE;

typedef struct
{
    uint32_t MessageType;
    uint32_t Padding;
} __attribute__((packed)) VMBUS_MESSAGE_HEADER;

typedef struct
{
    VMBUS_MESSAGE_HEADER Header;
    uint32_t Version;
    uint32_t TargetVcpu;
    union
    {
        uint64_t InterruptPage;
        struct
        {
            uint8_t MessageSint;
            uint8_t MessageVtl;
            uint8_t Reserved[2];
            uint32_t FeatureFlags;
        };
    };
    uint64_t MonitorPage1;
    uint64_t MonitorPage2;
} __attribute__((packed)) VMBUS_CHANNEL_INITIATE_CONTACT;

typedef struct
{
    VMBUS_MESSAGE_HEADER Header;
    uint8_t VersionSupported;
    uint8_t ConnectionState;
    uint16_t Padding;
    uint32_t MessageConnectionId;
} __attribute__((packed)) VMBUS_CHANNEL_VERSION_RESPONSE;

typedef struct
{
    VMBUS_GUID InterfaceType;
    VMBUS_GUID InterfaceInstance;
    uint64_t Reserved1;
    uint64_t Reserved2;
    uint16_t ChannelFlags;
    uint16_t MmioMegabytes;
    uint8_t UserData[120];
    uint16_t SubChannelIndex;
    uint16_t Reserved3;
} __attribute__((packed)) VMBUS_CHANNEL_OFFER;

typedef struct
{
    VMBUS_MESSAGE_HEADER Header;
    VMBUS_CHANNEL_OFFER Offer;
    uint32_t ChildRelId;
    uint8_t MonitorId;
    uint8_t MonitorAllocated;
    uint16_t IsDedicatedInterrupt;
    uint32_t ConnectionId;
} __attribute__((packed)) VMBUS_CHANNEL_OFFER_CHANNEL;

typedef struct
{
    VMBUS_MESSAGE_HEADER Header;
} __attribute__((packed)) VMBUS_CHANNEL_REQUEST_OFFERS;

typedef struct
{
    uint32_t ByteCount;
    uint32_t ByteOffset;
    uint64_t PfnArray[GPADL_MAX_PFN_COUNT];
} __attribute__((packed)) VMBUS_GPA_RANGE;

typedef struct
{
    VMBUS_MESSAGE_HEADER Header;
    uint32_t ChannelId;
    uint32_t GpadlHandle;
    uint16_t RangeBufLen;
    uint16_t RangeCount;
    VMBUS_GPA_RANGE Range;
} __attribute__((packed)) VMBUS_CHANNEL_GPADL_HEADER;

typedef struct
{
    VMBUS_MESSAGE_HEADER Header;
    uint32_t ChannelId;
    uint32_t GpadlHandle;
    uint32_t CreationStatus;
} __attribute__((packed)) VMBUS_CHANNEL_GPADL_CREATED;

typedef struct
{
    VMBUS_MESSAGE_HEADER Header;
    uint32_t ChannelId;
    uint32_t OpenId;
    uint32_t GpadlHandle;
    uint32_t TargetVcpu;
    uint32_t DownstreamRingBufferPageOffset;
    uint8_t UserData[120];
} __attribute__((packed)) VMBUS_CHANNEL_OPEN_CHANNEL;

typedef struct
{
    VMBUS_MESSAGE_HEADER Header;
    uint32_t ChannelId;
    uint32_t OpenId;
    uint32_t Status;
} __attribute__((packed)) VMBUS_CHANNEL_OPEN_RESULT;

typedef struct
{
    uint16_t Type;
    uint16_t Offset8;
    uint16_t Length8;
    uint16_t Flags;
    uint64_t TransactionId;
} __attribute__((packed)) VMBUS_PACKET_DESCRIPTOR;

static VMBUS_CHANNEL VMBusChannels[VMBUS_MAX_CHANNELS];
static void *VMBusMessagePage = NULL;
static void *VMBusEventPage = NULL;
static void *VMBusPostPage = NULL;
static void *VMBusMonitorPage1 = NULL;
static void *VMBusMonitorPage2 = NULL;
static uint32_t VMBusMessageConnectionId = VMBUS_MESSAGE_CONNECTION_ID_4;
static volatile bool ContactResponseReceived = false;
static volatile bool OffersDeliveredReceived = false;
static volatile bool GpadlCreatedReceived = false;
static volatile bool OpenChannelReceived = false;
static volatile uint32_t LastGpadlStatus = 0;
static volatile uint32_t LastOpenStatus = 0;

static void InterruptHandler(void); // SynIC 中断处理函数。
static bool PostMessage(const void *payload, uint32_t size); // 向宿主机发送 VMBus 控制消息。
static void NotifyHost(uint32_t channel_id); // 向宿主机发送某个通道的事件通知。
static void ProcessControlMessage(volatile HV_MESSAGE *message); // 处理 SynIC 消息页中的控制消息。
static void ProcessEventFlags(void); // 处理通道事件位图。
static void SaveOfferedChannel(const VMBUS_CHANNEL_OFFER_CHANNEL *offer); // 记录宿主机提供的一个通道。
static bool WaitForFlag(volatile bool *flag); // 等待某个异步标志变成 true。
static bool GuidEquals(const VMBUS_GUID *left, const VMBUS_GUID *right); // 比较两个 VMBus GUID。
static uint32_t GetRingDataSize(const VMBUS_CHANNEL *channel); // 计算环形缓冲区真实可用的数据区大小。
// 往环形缓冲区写入任意字节序列。
static uint32_t CopyToRing(VMBUS_RING_BUFFER *ring, uint32_t data_size, uint32_t offset, const void *buffer, uint32_t size);
// 从环形缓冲区读取任意字节序列。
static uint32_t CopyFromRing(const VMBUS_RING_BUFFER *ring, uint32_t data_size, uint32_t offset, void *buffer, uint32_t size);
static uint32_t Align8(uint32_t value); // 对齐到 8 字节边界。

void InitVMBus(void)
{
    for (uint32_t i = 0; i < VMBUS_MAX_CHANNELS; i++)
        memset(&VMBusChannels[i], 0, sizeof(VMBusChannels[i]));

    VMBusMessagePage = AllocatePage(PAGE_SIZE);
    VMBusEventPage = AllocatePage(PAGE_SIZE);
    VMBusPostPage = AllocatePage(PAGE_SIZE);
    VMBusMonitorPage1 = AllocatePage(PAGE_SIZE);
    VMBusMonitorPage2 = AllocatePage(PAGE_SIZE);

    if (VMBusMessagePage == NULL ||
        VMBusEventPage == NULL ||
        VMBusPostPage == NULL ||
        VMBusMonitorPage1 == NULL ||
        VMBusMonitorPage2 == NULL)
        return;

    SetupHypervisor(GetPhysicalAddress(VMBusMessagePage), GetPhysicalAddress(VMBusEventPage));
    SetInterruptHandler(VMBUS_INTERRUPT_VECTOR, InterruptHandler);

    VMBUS_CHANNEL_INITIATE_CONTACT contact;
    memset(&contact, 0, sizeof(contact));
    contact.Header.MessageType = VMBUS_MSG_INITIATE_CONTACT;
    contact.Version = VMBUS_VERSION_WIN10_V5;
    contact.TargetVcpu = 0;
    contact.MessageSint = HV_MESSAGE_SINT;
    contact.MessageVtl = 0;
    contact.MonitorPage1 = GetPhysicalAddress(VMBusMonitorPage1);
    contact.MonitorPage2 = GetPhysicalAddress(VMBusMonitorPage2);

    ContactResponseReceived = false;
    VMBusMessageConnectionId = VMBUS_MESSAGE_CONNECTION_ID_4;
    if (!PostMessage(&contact, sizeof(contact)))
        return;

    if (!WaitForFlag(&ContactResponseReceived))
        return;

    VMBUS_CHANNEL_REQUEST_OFFERS requestOffers;
    memset(&requestOffers, 0, sizeof(requestOffers));
    requestOffers.Header.MessageType = VMBUS_MSG_REQUEST_OFFERS;

    OffersDeliveredReceived = false;
    if (PostMessage(&requestOffers, sizeof(requestOffers)))
        (void)WaitForFlag(&OffersDeliveredReceived);
}

bool VMBusOpenChannel(uint32_t channel_id, uint32_t buffer_size, void (*callback)(void *))
{
    if (channel_id >= VMBUS_MAX_CHANNELS || buffer_size < PAGE_SIZE * 2)
        return false;

    VMBUS_CHANNEL *channel = &VMBusChannels[channel_id];
    if (!channel->Offered || channel->IsOpen)
        return false;

    uint32_t pageCountPerRing = (buffer_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t totalPageCount = pageCountPerRing * 2;
    if (totalPageCount > GPADL_MAX_PFN_COUNT)
        return false;

    void *outbound = AllocatePage(pageCountPerRing * PAGE_SIZE);
    void *inbound = AllocatePage(pageCountPerRing * PAGE_SIZE);
    if (outbound == NULL || inbound == NULL)
        return false;

    channel->Callback = callback;
    channel->OutboundBuffer = (VMBUS_RING_BUFFER *)outbound;
    channel->InboundBuffer = (VMBUS_RING_BUFFER *)inbound;
    channel->RingBufferSize = pageCountPerRing * PAGE_SIZE;
    channel->GpadlHandle = GPADL_HANDLE_BASE + channel_id;

    memset(channel->OutboundBuffer, 0, channel->RingBufferSize);
    memset(channel->InboundBuffer, 0, channel->RingBufferSize);

    VMBUS_CHANNEL_GPADL_HEADER gpadl;
    memset(&gpadl, 0, sizeof(gpadl));
    gpadl.Header.MessageType = VMBUS_MSG_GPADL_HEADER;
    gpadl.ChannelId = channel_id;
    gpadl.GpadlHandle = channel->GpadlHandle;
    gpadl.RangeCount = 1;
    gpadl.Range.ByteCount = channel->RingBufferSize * 2;
    gpadl.Range.ByteOffset = 0;
    gpadl.RangeBufLen = (uint16_t)(sizeof(uint32_t) * 2 + totalPageCount * sizeof(uint64_t));

    for (uint32_t page = 0; page < pageCountPerRing; page++)
        gpadl.Range.PfnArray[page] =
            GetPhysicalAddress((uint8_t *)outbound + page * PAGE_SIZE) >> 12;

    for (uint32_t page = 0; page < pageCountPerRing; page++)
        gpadl.Range.PfnArray[pageCountPerRing + page] =
            GetPhysicalAddress((uint8_t *)inbound + page * PAGE_SIZE) >> 12;

    GpadlCreatedReceived = false;
    LastGpadlStatus = 0xFFFFFFFFu;
    if (!PostMessage(&gpadl, sizeof(VMBUS_CHANNEL_GPADL_HEADER) - sizeof(gpadl.Range.PfnArray) + totalPageCount * sizeof(uint64_t)))
        return false;

    if (!WaitForFlag(&GpadlCreatedReceived) || LastGpadlStatus != 0)
        return false;

    VMBUS_CHANNEL_OPEN_CHANNEL open;
    memset(&open, 0, sizeof(open));
    open.Header.MessageType = VMBUS_MSG_OPEN_CHANNEL;
    open.ChannelId = channel_id;
    open.OpenId = channel_id;
    open.GpadlHandle = channel->GpadlHandle;
    open.TargetVcpu = 0;
    open.DownstreamRingBufferPageOffset = pageCountPerRing;

    OpenChannelReceived = false;
    LastOpenStatus = 0xFFFFFFFFu;
    if (!PostMessage(&open, sizeof(open)))
        return false;

    if (!WaitForFlag(&OpenChannelReceived) || LastOpenStatus != 0)
        return false;

    channel->IsOpen = true;
    return true;
}

void VMBusWriteChannel(uint32_t channel_id, const void *buffer, uint32_t size)
{
    (void)VMBusSendPacket(
        channel_id,
        buffer,
        size,
        (uint64_t)(uintptr_t)buffer,
        VMBUS_PACKET_DATA_INBAND,
        0);
}

bool VMBusFindChannelByGuid(const VMBUS_GUID *guid, uint32_t *channel_id)
{
    if (guid == NULL || channel_id == NULL)
        return false;

    for (uint32_t index = 0; index < VMBUS_MAX_CHANNELS; index++)
    {
        if (VMBusChannels[index].Offered &&
            GuidEquals(&VMBusChannels[index].DeviceGuid, guid))
        {
            *channel_id = index;
            return true;
        }
    }

    return false;
}

bool VMBusSendPacket(
    uint32_t channel_id,
    const void *buffer,
    uint32_t size,
    uint64_t request_id,
    uint16_t packet_type,
    uint16_t flags)
{
    if (channel_id >= VMBUS_MAX_CHANNELS || buffer == NULL)
        return false;

    VMBUS_CHANNEL *channel = &VMBusChannels[channel_id];
    if (!channel->IsOpen || channel->OutboundBuffer == NULL)
        return false;

    uint32_t dataSize = GetRingDataSize(channel);
    uint32_t packetSize = Align8(sizeof(VMBUS_PACKET_DESCRIPTOR) + size);
    uint32_t totalSize = packetSize + sizeof(uint64_t);
    VMBUS_RING_BUFFER *ring = channel->OutboundBuffer;

    uint32_t readIndex = ring->ReadIndex;
    uint32_t writeIndex = ring->WriteIndex;
    uint32_t used = writeIndex >= readIndex ? writeIndex - readIndex : dataSize - readIndex + writeIndex;
    if (dataSize - used <= totalSize)
        return false;

    VMBUS_PACKET_DESCRIPTOR descriptor;
    descriptor.Type = packet_type;
    descriptor.Offset8 = sizeof(VMBUS_PACKET_DESCRIPTOR) / 8;
    descriptor.Length8 = (uint16_t)(packetSize / 8);
    descriptor.Flags = flags;
    descriptor.TransactionId = request_id;

    uint64_t previousIndices = ((uint64_t)writeIndex << 32) | readIndex;
    uint32_t next = writeIndex;
    next = CopyToRing(ring, dataSize, next, &descriptor, sizeof(descriptor));
    next = CopyToRing(ring, dataSize, next, buffer, size);

    uint8_t zero = 0;
    for (uint32_t pad = sizeof(descriptor) + size; pad < packetSize; pad++)
        next = CopyToRing(ring, dataSize, next, &zero, sizeof(zero));

    next = CopyToRing(ring, dataSize, next, &previousIndices, sizeof(previousIndices));
    ring->WriteIndex = next;

    NotifyHost(channel_id);
    return true;
}

bool VMBusReadPacket(
    uint32_t channel_id,
    void *buffer,
    uint32_t buffer_size,
    uint32_t *bytes_read,
    uint64_t *request_id)
{
    if (channel_id >= VMBUS_MAX_CHANNELS || buffer == NULL)
        return false;

    VMBUS_CHANNEL *channel = &VMBusChannels[channel_id];
    if (!channel->IsOpen || channel->InboundBuffer == NULL)
        return false;

    VMBUS_RING_BUFFER *ring = channel->InboundBuffer;
    uint32_t dataSize = GetRingDataSize(channel);
    uint32_t readIndex = ring->ReadIndex;
    uint32_t writeIndex = ring->WriteIndex;

    if (readIndex == writeIndex)
        return false;

    VMBUS_PACKET_DESCRIPTOR descriptor;
    (void)CopyFromRing(ring, dataSize, readIndex, &descriptor, sizeof(descriptor));

    uint32_t packetSize = descriptor.Length8 * 8;
    uint32_t payloadOffset = descriptor.Offset8 * 8;
    if (packetSize < payloadOffset || packetSize - payloadOffset > buffer_size)
        return false;

    uint32_t payloadSize = packetSize - payloadOffset;
    (void)CopyFromRing(ring, dataSize, (readIndex + payloadOffset) % dataSize, buffer, payloadSize);

    ring->ReadIndex = (readIndex + packetSize + sizeof(uint64_t)) % dataSize;

    if (bytes_read != NULL)
        *bytes_read = payloadSize;
    if (request_id != NULL)
        *request_id = descriptor.TransactionId;

    return true;
}

static void InterruptHandler(void)
{
    volatile HV_MESSAGE *message =
        (volatile HV_MESSAGE *)((uint8_t *)VMBusMessagePage + HV_MESSAGE_SIZE * HV_MESSAGE_SINT);

    if (message->MessageType != 0)
        ProcessControlMessage(message);

    ProcessEventFlags();
}

static bool PostMessage(const void *payload, uint32_t size)
{
    if (VMBusPostPage == NULL || payload == NULL || size > HV_MESSAGE_PAYLOAD_SIZE)
        return false;

    HV_POST_MESSAGE_INPUT *input = (HV_POST_MESSAGE_INPUT *)VMBusPostPage;
    memset(input, 0, sizeof(*input));
    input->ConnectionId = VMBusMessageConnectionId;
    input->MessageType = 1;
    input->PayloadSize = size;
    memcpy(input->Payload, payload, size);

    return Hypercall(HVCALL_POST_MESSAGE, GetPhysicalAddress(VMBusPostPage)) == HV_STATUS_SUCCESS;
}

static void NotifyHost(uint32_t channel_id)
{
    if (channel_id >= VMBUS_MAX_CHANNELS)
        return;

    VMBUS_CHANNEL *channel = &VMBusChannels[channel_id];
    if (!channel->Offered)
        return;

    (void)Hypercall(
        HVCALL_SIGNAL_EVENT | HV_HYPERCALL_FAST_BIT,
        channel->ConnectionId != 0 ? channel->ConnectionId : VMBUS_EVENT_CONNECTION_ID);
}

static void ProcessControlMessage(volatile HV_MESSAGE *message)
{
    VMBUS_MESSAGE_HEADER *header = (VMBUS_MESSAGE_HEADER *)message->Payload;

    switch (header->MessageType)
    {
    case VMBUS_MSG_VERSION_RESPONSE:
    {
        VMBUS_CHANNEL_VERSION_RESPONSE *response = (VMBUS_CHANNEL_VERSION_RESPONSE *)message->Payload;
        if (response->MessageConnectionId != 0)
            VMBusMessageConnectionId = response->MessageConnectionId;
        ContactResponseReceived = response->VersionSupported != 0;
        break;
    }
    case VMBUS_MSG_OFFER_CHANNEL:
        SaveOfferedChannel((const VMBUS_CHANNEL_OFFER_CHANNEL *)message->Payload);
        break;
    case VMBUS_MSG_ALL_OFFERS_DELIVERED:
        OffersDeliveredReceived = true;
        break;
    case VMBUS_MSG_GPADL_CREATED:
    {
        VMBUS_CHANNEL_GPADL_CREATED *created = (VMBUS_CHANNEL_GPADL_CREATED *)message->Payload;
        LastGpadlStatus = created->CreationStatus;
        GpadlCreatedReceived = true;
        break;
    }
    case VMBUS_MSG_OPEN_CHANNEL_RESULT:
    {
        VMBUS_CHANNEL_OPEN_RESULT *result = (VMBUS_CHANNEL_OPEN_RESULT *)message->Payload;
        LastOpenStatus = result->Status;
        OpenChannelReceived = true;
        break;
    }
    default:
        break;
    }

    uint32_t oldType = message->MessageType;
    message->MessageType = 0;

    if ((message->MessageFlags & 1u) != 0 || oldType != 0)
        AckHyperMessage();
}

static void ProcessEventFlags(void)
{
    volatile uint64_t *eventBits =
        (volatile uint64_t *)((uint8_t *)VMBusEventPage + HV_MESSAGE_SIZE * HV_MESSAGE_SINT);

    for (uint32_t channel = 1; channel < VMBUS_MAX_CHANNELS; channel++)
    {
        uint64_t mask = 1ull << (channel % 64);
        volatile uint64_t *word = &eventBits[channel / 64];
        if ((*word & mask) == 0)
            continue;

        *word &= ~mask;
        if (VMBusChannels[channel].IsOpen && VMBusChannels[channel].Callback != NULL)
            VMBusChannels[channel].Callback(&VMBusChannels[channel]);
    }
}

static void SaveOfferedChannel(const VMBUS_CHANNEL_OFFER_CHANNEL *offer)
{
    if (offer == NULL || offer->ChildRelId >= VMBUS_MAX_CHANNELS)
        return;

    VMBUS_CHANNEL *channel = &VMBusChannels[offer->ChildRelId];
    memset(channel, 0, sizeof(*channel));
    channel->ChannelId = offer->ChildRelId;
    channel->DeviceGuid = offer->Offer.InterfaceType;
    channel->ConnectionId = offer->ConnectionId;
    channel->Offered = true;
}

static bool WaitForFlag(volatile bool *flag)
{
    for (uint32_t retry = 0; retry < VMBUS_WAIT_TIMEOUT; retry++)
    {
        if (*flag)
            return true;
        Pause();
    }

    return false;
}

static bool GuidEquals(const VMBUS_GUID *left, const VMBUS_GUID *right)
{
    return memcmp(left, right, sizeof(VMBUS_GUID)) == 0;
}

static uint32_t GetRingDataSize(const VMBUS_CHANNEL *channel)
{
    if (channel == NULL || channel->RingBufferSize <= PAGE_SIZE)
        return 0;

    return channel->RingBufferSize - PAGE_SIZE;
}

static uint32_t CopyToRing(VMBUS_RING_BUFFER *ring, uint32_t data_size, uint32_t offset, const void *buffer, uint32_t size)
{
    const uint8_t *source = (const uint8_t *)buffer;

    for (uint32_t index = 0; index < size; index++)
    {
        ring->Buffer[offset] = source[index];
        offset = (offset + 1) % data_size;
    }

    return offset;
}

static uint32_t CopyFromRing(const VMBUS_RING_BUFFER *ring, uint32_t data_size, uint32_t offset, void *buffer, uint32_t size)
{
    uint8_t *target = (uint8_t *)buffer;

    for (uint32_t index = 0; index < size; index++)
    {
        target[index] = ring->Buffer[offset];
        offset = (offset + 1) % data_size;
    }

    return offset;
}

static uint32_t Align8(uint32_t value)
{
    return (value + 7u) & ~7u;
}
