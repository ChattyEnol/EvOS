/** Drivers/VM/VM_Keyboard.c
 *
 * (C) Charity Enol
 *
 * VMBus 虚拟键盘驱动实现。
 */

#include <Drivers/Keyboard.h>
#include <Drivers/VM/VMBus.h>
#include <Drivers/VM/VM_Keyboard.h>
#include <stddef.h>

#define VMBUS_KEYBOARD_CHANNEL_ID 3
#define VMBUS_KEYBOARD_BUFFER_SIZE 4096

typedef struct
{
    uint16_t MakeCode;
    uint16_t IsPressed;
} __attribute__((packed)) VMBUS_KEYBOARD_PACKET;

static void VMKeyboardCallback(void *data);
static KEY_CODE MapMakeCodeToKeyCode(uint16_t make_code);

bool InitVMKeyboard(void)
{
    return VMBusOpenChannel(VMBUS_KEYBOARD_CHANNEL_ID, VMBUS_KEYBOARD_BUFFER_SIZE, VMKeyboardCallback);
}

static void VMKeyboardCallback(void *data)
{
    VMBUS_CHANNEL *channel = (VMBUS_CHANNEL *)data;
    if (channel == NULL || channel->InboundBuffer == NULL)
        return;

    VMBUS_RING_BUFFER *ring = channel->InboundBuffer;

    while (ring->ReadIndex != ring->WriteIndex)
    {
        VMBUS_KEYBOARD_PACKET packet;
        uint8_t *packet_bytes = (uint8_t *)&packet;
        uint32_t current_read = ring->ReadIndex;

        for (uint32_t i = 0; i < sizeof(VMBUS_KEYBOARD_PACKET); i++)
        {
            packet_bytes[i] = ring->Buffer[current_read];
            current_read = (current_read + 1) % VMBUS_KEYBOARD_BUFFER_SIZE;
        }

        ring->ReadIndex = current_read;

        KEY_CODE key_code = MapMakeCodeToKeyCode(packet.MakeCode);
        if (key_code != KEY_NONE)
            SubmitKeyboardEvent(key_code, packet.IsPressed != 0);
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