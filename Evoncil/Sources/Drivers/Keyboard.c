/** Drivers/Keyboard.c
 *
 * (C) Charity Enol
 *
 * 键盘驱动实现。
 * 只负责接收中断、解析 PS/2 Set 2 扫描码状态机，并塞入环形缓冲区。
 * 没有任何字符转换，没有任何业务逻辑！
 */

#include <HAL/HAL.h>
#include <Drivers/Keyboard.h>
#include <stddef.h>

// 调试用。
#include <UI/TextIO.h>

/**
 * 硬件端口定义。
 */

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_COMMAND_PORT 0x64
#define KEYBOARD_OUTPUT_READY 0x01
#define KEYBOARD_INPUT_BUSY 0x02
#define KEYBOARD_COMMAND_READ_CONFIG 0x20
#define KEYBOARD_COMMAND_WRITE_CONFIG 0x60
#define KEYBOARD_COMMAND_ENABLE_FIRST_PORT 0xAE
#define KEYBOARD_COMMAND_ENABLE_SCANNING 0xF4
#define KEYBOARD_CONFIG_FIRST_PORT_INTERRUPT 0x01
#define KEYBOARD_CONFIG_FIRST_PORT_CLOCK_DISABLED 0x10
#define KEYBOARD_CONFIG_TRANSLATION 0x40

// 环形缓冲区大小（必须是 2 的幂次，方便或者刚好够用）。
#define KEYBOARD_BUFFER_SIZE 256

// 环形缓冲区。
static KEYBOARD_EVENT KeyboardBuffer[KEYBOARD_BUFFER_SIZE];
static uint32_t BufferHead = 0; // 写入位置
static uint32_t BufferTail = 0; // 读取位置

// PS/2 状态机。
typedef enum
{
    STATE_NORMAL,               // 正常状态
    STATE_EXPECT_BREAK,         // 收到 0xF0，期待下一个是断码
    STATE_EXTENDED,             // 收到 0xE0，期待扩展码
    STATE_EXTENDED_EXPECT_BREAK // 收到 0xE0 然后收到 0xF0
} PS2_STATE;

static PS2_STATE CurrentState = STATE_NORMAL;

static bool WaitKeyboardRead(void);
static bool WaitKeyboardWrite(void);
static bool ReadKeyboardControllerData(uint8_t *data);
static bool WriteKeyboardControllerCommand(uint8_t command);
static bool WriteKeyboardControllerData(uint8_t data);
static void InitKeyboardController(void);

// 扫描码映射表。

// 普通按键。
static const KEY_CODE NormalMap[256] = {
    // 字母。
    [0x1C] = KEY_A,
    [0x32] = KEY_B,
    [0x21] = KEY_C,
    [0x23] = KEY_D,
    [0x24] = KEY_E,
    [0x2B] = KEY_F,
    [0x34] = KEY_G,
    [0x33] = KEY_H,
    [0x43] = KEY_I,
    [0x3B] = KEY_J,
    [0x42] = KEY_K,
    [0x4B] = KEY_L,
    [0x3A] = KEY_M,
    [0x31] = KEY_N,
    [0x44] = KEY_O,
    [0x4D] = KEY_P,
    [0x15] = KEY_Q,
    [0x2D] = KEY_R,
    [0x1B] = KEY_S,
    [0x2C] = KEY_T,
    [0x3C] = KEY_U,
    [0x2A] = KEY_V,
    [0x1D] = KEY_W,
    [0x22] = KEY_X,
    [0x35] = KEY_Y,
    [0x1A] = KEY_Z,

    // 数字与符号。
    [0x0E] = KEY_GRAVE,
    [0x16] = KEY_1,
    [0x1E] = KEY_2,
    [0x26] = KEY_3,
    [0x25] = KEY_4,
    [0x2E] = KEY_5,
    [0x36] = KEY_6,
    [0x3D] = KEY_7,
    [0x3E] = KEY_8,
    [0x46] = KEY_9,
    [0x45] = KEY_0,
    [0x4E] = KEY_MINUS,
    [0x55] = KEY_EQUAL,
    [0x66] = KEY_BACKSPACE,

    // 其他符号。
    [0x54] = KEY_LBRACKET,
    [0x5B] = KEY_RBRACKET,
    [0x5D] = KEY_BACKSLASH,
    [0x4C] = KEY_SEMICOLON,
    [0x52] = KEY_QUOTE,
    [0x41] = KEY_COMMA,
    [0x49] = KEY_PERIOD,
    [0x4A] = KEY_SLASH,

    // 控制键与功能键。
    [0x5A] = KEY_ENTER,
    [0x29] = KEY_SPACE,
    [0x0D] = KEY_TAB,
    [0x76] = KEY_ESC,
    [0x58] = KEY_CAPS_LOCK,

    // 左右融为一体的修饰键。
    [0x12] = KEY_SHIFT,
    [0x59] = KEY_SHIFT, // 左/右 Shift
    [0x14] = KEY_CTRL,  // 左 Ctrl
    [0x11] = KEY_ALT,   // 左 Alt

    // F1 - F12
    [0x05] = KEY_F1,
    [0x06] = KEY_F2,
    [0x04] = KEY_F3,
    [0x0C] = KEY_F4,
    [0x03] = KEY_F5,
    [0x0B] = KEY_F6,
    [0x83] = KEY_F7,
    [0x0A] = KEY_F8,
    [0x01] = KEY_F9,
    [0x09] = KEY_F10,
    [0x78] = KEY_F11,
    [0x07] = KEY_F12,
};

// 扩展按键映射表（前缀为 0xE0 的按键）。
static const KEY_CODE ExtendedMap[256] = {
    [0x75] = KEY_UP,
    [0x72] = KEY_DOWN,
    [0x6B] = KEY_LEFT,
    [0x74] = KEY_RIGHT,
    [0x71] = KEY_DELETE,
    [0x14] = KEY_CTRL, // 右 Ctrl
    [0x11] = KEY_ALT,  // 右 Alt
    [0x1F] = KEY_WIN,
    [0x27] = KEY_WIN,   // 左/右 Win
    [0x4A] = KEY_SLASH, // 小键盘 / 映射为普通 /
    [0x5A] = KEY_ENTER, // 小键盘 Enter 映射为普通 Enter
};

/**
 * 中断处理逻辑。
 */

// 没有参数的函数，匹配 HAL.h 里的 HandleInterrupt。
static void KeyboardInterruptHandler(void)
{
    // 从硬件端口读取原始扫描码。
    uint8_t scanCode = ReadHardwarePortByte(KEYBOARD_DATA_PORT);
    KEY_CODE keyCode = KEY_NONE;
    bool pressed = true;

    // 微型状态机，解包变长硬件码。
    switch (CurrentState)
    {
    case STATE_NORMAL:
        if (scanCode == 0xE0)
        {
            CurrentState = STATE_EXTENDED;
            goto done;
        }
        else if (scanCode == 0xF0)
        {
            CurrentState = STATE_EXPECT_BREAK;
            goto done;
        }
        keyCode = NormalMap[scanCode];
        pressed = true;
        break;

    case STATE_EXPECT_BREAK:
        keyCode = NormalMap[scanCode];
        pressed = false;
        CurrentState = STATE_NORMAL;
        break;

    case STATE_EXTENDED:
        if (scanCode == 0xF0)
        {
            CurrentState = STATE_EXTENDED_EXPECT_BREAK;
            goto done;
        }
        keyCode = ExtendedMap[scanCode];
        pressed = true;
        CurrentState = STATE_NORMAL;
        break;

    case STATE_EXTENDED_EXPECT_BREAK:
        keyCode = ExtendedMap[scanCode];
        pressed = false;
        CurrentState = STATE_NORMAL;
        break;
    }

    // 如果成功解析出了按键，塞进环形缓冲区。
    if (keyCode != KEY_NONE)
    {
        uint32_t nextHead = (BufferHead + 1) % KEYBOARD_BUFFER_SIZE;
        // 如果队列没满，就往里写（满了就只能丢弃这个按键了，防止覆盖未读数据）。
        if (nextHead != BufferTail)
        {
            KeyboardBuffer[BufferHead].KeyCode = keyCode;
            KeyboardBuffer[BufferHead].Pressed = pressed;
            BufferHead = nextHead;
        }
    }

done:
    return;
}

/* ================== 对外接口 ================== */

void InitKeyboard(void)
{
    // 清空缓冲区状态。
    BufferHead = 0;
    BufferTail = 0;
    CurrentState = STATE_NORMAL;

    kprintf("Buffer cleared.\n");

    // 让 PS/2 控制器真的把键盘中断送出来。
    InitKeyboardController();

    // 向 HAL 注册 33 号中断（IRQ1）的处理函数。
    SetInterruptGate(33, KeyboardInterruptHandler);
    SetHardwareInterrupt(1, 33);
}

bool ReadKeyboard(KEYBOARD_EVENT *event)
{
    if (event == NULL)
        return false;

    // 如果头尾指针相等，说明没有新按键
    if (BufferHead == BufferTail)
        return false;

    // 关闭中断，防止在读取缓冲区的时候触发中断导致数据竞争
    DisableInterrupts();

    *event = KeyboardBuffer[BufferTail];
    BufferTail = (BufferTail + 1) % KEYBOARD_BUFFER_SIZE;

    EnableInterrupts();

    return true;
}

static bool WaitKeyboardRead(void)
{
    for (uint32_t timeout = 0; timeout < 100000; timeout++)
        if ((ReadHardwarePortByte(KEYBOARD_STATUS_PORT) & KEYBOARD_OUTPUT_READY) != 0)
            return true;

    return false;
}

static bool WaitKeyboardWrite(void)
{
    for (uint32_t timeout = 0; timeout < 100000; timeout++)
        if ((ReadHardwarePortByte(KEYBOARD_STATUS_PORT) & KEYBOARD_INPUT_BUSY) == 0)
            return true;

    return false;
}

static bool ReadKeyboardControllerData(uint8_t *data)
{
    if (data == NULL || !WaitKeyboardRead())
        return false;

    *data = ReadHardwarePortByte(KEYBOARD_DATA_PORT);
    return true;
}

static bool WriteKeyboardControllerCommand(uint8_t command)
{
    if (!WaitKeyboardWrite())
        return false;

    WriteHardwarePortByte(KEYBOARD_COMMAND_PORT, command);
    return true;
}

static bool WriteKeyboardControllerData(uint8_t data)
{
    if (!WaitKeyboardWrite())
        return false;

    WriteHardwarePortByte(KEYBOARD_DATA_PORT, data);
    return true;
}

static void InitKeyboardController(void)
{
    uint8_t config = 0;

    WriteKeyboardControllerCommand(KEYBOARD_COMMAND_ENABLE_FIRST_PORT);

    if (WriteKeyboardControllerCommand(KEYBOARD_COMMAND_READ_CONFIG) &&
        ReadKeyboardControllerData(&config))
    {
        config |= KEYBOARD_CONFIG_FIRST_PORT_INTERRUPT;
        config &= (uint8_t)~KEYBOARD_CONFIG_FIRST_PORT_CLOCK_DISABLED;
        config &= (uint8_t)~KEYBOARD_CONFIG_TRANSLATION;

        if (WriteKeyboardControllerCommand(KEYBOARD_COMMAND_WRITE_CONFIG))
            WriteKeyboardControllerData(config);
    }

    WriteKeyboardControllerData(KEYBOARD_COMMAND_ENABLE_SCANNING);

    if (WaitKeyboardRead())
        (void)ReadHardwarePortByte(KEYBOARD_DATA_PORT);
}
