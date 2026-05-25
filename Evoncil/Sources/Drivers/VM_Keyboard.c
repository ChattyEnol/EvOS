/** Drivers/VM/VM_Keyboard.c
 *
 * (C) Charity Enol
 *
 * VMBus 虚拟键盘驱动实现。
 */

#include <Drivers/Keyboard.h>
#include <Drivers/VM/VMBus.h>
#include <Drivers/VM/VM_Keyboard.h>
#include <HAL/HAL.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define VMBUS_KEYBOARD_BUFFER_SIZE (10 * 4096)
#define SYNTH_KBD_VERSION 0x00010000u
#define SYNTH_KBD_PROTOCOL_REQUEST 1
#define SYNTH_KBD_PROTOCOL_RESPONSE 2
#define SYNTH_KBD_EVENT 3
#define SYNTH_KBD_PROTOCOL_ACCEPTED 1u
#define SYNTH_KBD_IS_BREAK (1u << 1)
#define SYNTH_KBD_IS_E0 (1u << 2)
#define SYNTH_KBD_IS_E1 (1u << 3)
#define VMBUS_PACKET_DATA_INBAND 0x6
#define VMBUS_PACKET_COMPLETION_REQUESTED 0x1

static const VMBUS_GUID KeyboardGuid = {
    0xf912ad6d,
    0x2b17,
    0x48ea,
    {0xbd, 0x65, 0xf9, 0x27, 0xa6, 0x1c, 0x76, 0x84}};

typedef struct
{
    uint32_t Type;
} __attribute__((packed)) SYNTH_KBD_HEADER;

typedef struct
{
    SYNTH_KBD_HEADER Header;
    uint32_t Version;
} __attribute__((packed)) SYNTH_KBD_PROTOCOL_REQUEST_MESSAGE;

typedef struct
{
    SYNTH_KBD_HEADER Header;
    uint32_t Status;
} __attribute__((packed)) SYNTH_KBD_PROTOCOL_RESPONSE_MESSAGE;

typedef struct
{
    SYNTH_KBD_HEADER Header;
    uint16_t MakeCode;
    uint16_t Reserved;
    uint32_t Info;
} __attribute__((packed)) SYNTH_KBD_EVENT_MESSAGE;

static uint32_t KeyboardChannelId = 0;
static volatile bool KeyboardProtocolReady = false;

static void VMKeyboardCallback(void *data);
static bool ConnectKeyboardProtocol(void);
static bool WaitKeyboardProtocol(void);
static void HandleKeyboardMessage(const void *message, uint32_t size);
static KEY_CODE MapMakeCodeToKeyCode(uint16_t make_code);

bool InitVMKeyboard(void)
{
    if (!VMBusFindChannelByGuid(&KeyboardGuid, &KeyboardChannelId))
        return false;

    if (!VMBusOpenChannel(KeyboardChannelId, VMBUS_KEYBOARD_BUFFER_SIZE, VMKeyboardCallback))
        return false;

    return ConnectKeyboardProtocol();
}

static void VMKeyboardCallback(void *data)
{
    VMBUS_CHANNEL *channel = (VMBUS_CHANNEL *)data;
    if (channel == NULL)
        return;

    uint8_t message[256];
    uint32_t messageSize;
    uint64_t requestId;
    while (VMBusReadPacket(channel->ChannelId, message, sizeof(message), &messageSize, &requestId))
    {
        (void)requestId;
        HandleKeyboardMessage(message, messageSize);
    }
}

static bool ConnectKeyboardProtocol(void)
{
    SYNTH_KBD_PROTOCOL_REQUEST_MESSAGE request;
    memset(&request, 0, sizeof(request));
    request.Header.Type = SYNTH_KBD_PROTOCOL_REQUEST;
    request.Version = SYNTH_KBD_VERSION;

    KeyboardProtocolReady = false;
    if (!VMBusSendPacket(
            KeyboardChannelId,
            &request,
            sizeof(request),
            (uint64_t)(uintptr_t)&request,
            VMBUS_PACKET_DATA_INBAND,
            VMBUS_PACKET_COMPLETION_REQUESTED))
        return false;

    return WaitKeyboardProtocol();
}

static bool WaitKeyboardProtocol(void)
{
    for (uint32_t retry = 0; retry < 100000000u; retry++)
    {
        if (KeyboardProtocolReady)
            return true;
        Pause();
    }

    return false;
}

static void HandleKeyboardMessage(const void *message, uint32_t size)
{
    if (message == NULL || size < sizeof(SYNTH_KBD_HEADER))
        return;

    const SYNTH_KBD_HEADER *header = (const SYNTH_KBD_HEADER *)message;
    switch (header->Type)
    {
    case SYNTH_KBD_PROTOCOL_RESPONSE:
    {
        if (size < sizeof(SYNTH_KBD_PROTOCOL_RESPONSE_MESSAGE))
            return;

        const SYNTH_KBD_PROTOCOL_RESPONSE_MESSAGE *response =
            (const SYNTH_KBD_PROTOCOL_RESPONSE_MESSAGE *)message;
        KeyboardProtocolReady = (response->Status & SYNTH_KBD_PROTOCOL_ACCEPTED) != 0;
        break;
    }
    case SYNTH_KBD_EVENT:
    {
        if (size < sizeof(SYNTH_KBD_EVENT_MESSAGE))
            return;

        const SYNTH_KBD_EVENT_MESSAGE *event = (const SYNTH_KBD_EVENT_MESSAGE *)message;
        uint16_t makeCode = event->MakeCode;
        if ((event->Info & SYNTH_KBD_IS_E0) != 0)
            makeCode |= 0xE000;
        if ((event->Info & SYNTH_KBD_IS_E1) != 0)
            makeCode |= 0xE100;

        KEY_CODE keyCode = MapMakeCodeToKeyCode(makeCode);
        if (keyCode != KEY_NONE)
            SubmitKeyboardEvent(keyCode, (event->Info & SYNTH_KBD_IS_BREAK) == 0);
        break;
    }
    default:
        break;
    }
}

static KEY_CODE MapMakeCodeToKeyCode(uint16_t make_code)
{
    switch (make_code)
    {
    case 0x01:
        return KEY_ESC;
    case 0x02:
        return KEY_1;
    case 0x03:
        return KEY_2;
    case 0x04:
        return KEY_3;
    case 0x05:
        return KEY_4;
    case 0x06:
        return KEY_5;
    case 0x07:
        return KEY_6;
    case 0x08:
        return KEY_7;
    case 0x09:
        return KEY_8;
    case 0x0A:
        return KEY_9;
    case 0x0B:
        return KEY_0;
    case 0x0C:
        return KEY_MINUS;
    case 0x0D:
        return KEY_EQUAL;
    case 0x0E:
        return KEY_BACKSPACE;
    case 0x0F:
        return KEY_TAB;
    case 0x10:
        return KEY_Q;
    case 0x11:
        return KEY_W;
    case 0x12:
        return KEY_E;
    case 0x13:
        return KEY_R;
    case 0x14:
        return KEY_T;
    case 0x15:
        return KEY_Y;
    case 0x16:
        return KEY_U;
    case 0x17:
        return KEY_I;
    case 0x18:
        return KEY_O;
    case 0x19:
        return KEY_P;
    case 0x1A:
        return KEY_LBRACKET;
    case 0x1B:
        return KEY_RBRACKET;
    case 0x1C:
        return KEY_ENTER;
    case 0x1D:
    case 0xE01D:
        return KEY_CTRL;
    case 0x1E:
        return KEY_A;
    case 0x1F:
        return KEY_S;
    case 0x20:
        return KEY_D;
    case 0x21:
        return KEY_F;
    case 0x22:
        return KEY_G;
    case 0x23:
        return KEY_H;
    case 0x24:
        return KEY_J;
    case 0x25:
        return KEY_K;
    case 0x26:
        return KEY_L;
    case 0x27:
        return KEY_SEMICOLON;
    case 0x28:
        return KEY_QUOTE;
    case 0x29:
        return KEY_GRAVE;
    case 0x2A:
    case 0x36:
        return KEY_SHIFT;
    case 0x2B:
        return KEY_BACKSLASH;
    case 0x2C:
        return KEY_Z;
    case 0x2D:
        return KEY_X;
    case 0x2E:
        return KEY_C;
    case 0x2F:
        return KEY_V;
    case 0x30:
        return KEY_B;
    case 0x31:
        return KEY_N;
    case 0x32:
        return KEY_M;
    case 0x33:
        return KEY_COMMA;
    case 0x34:
        return KEY_PERIOD;
    case 0x35:
        return KEY_SLASH;
    case 0x38:
    case 0xE038:
        return KEY_ALT;
    case 0x39:
        return KEY_SPACE;
    case 0x3A:
        return KEY_CAPS_LOCK;
    case 0x3B:
        return KEY_F1;
    case 0x3C:
        return KEY_F2;
    case 0x3D:
        return KEY_F3;
    case 0x3E:
        return KEY_F4;
    case 0x3F:
        return KEY_F5;
    case 0x40:
        return KEY_F6;
    case 0x41:
        return KEY_F7;
    case 0x42:
        return KEY_F8;
    case 0x43:
        return KEY_F9;
    case 0x44:
        return KEY_F10;
    case 0x57:
        return KEY_F11;
    case 0x58:
        return KEY_F12;
    case 0x48:
        return KEY_UP;
    case 0x50:
        return KEY_DOWN;
    case 0x4B:
        return KEY_LEFT;
    case 0x4D:
        return KEY_RIGHT;
    case 0x53:
        return KEY_DELETE;
    case 0xE05B:
    case 0xE05C:
        return KEY_WIN;
    default:
        return KEY_NONE;
    }
}
