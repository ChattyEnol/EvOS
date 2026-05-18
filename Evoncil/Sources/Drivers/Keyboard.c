/** Drivers/Keyboard.c
 *
 * (C) Charity Enol
 *
 * 现代键盘输入实现。
 * 本模块不再直接访问 PS/2 控制器，只接收 USB HID Boot Keyboard Report，
 * 然后把按键变化转换成 EvOS 统一的 KEY_CODE 事件。
 */

#include <Drivers/Keyboard.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define KEYBOARD_BUFFER_SIZE 256
#define HID_BOOT_REPORT_SIZE 8
#define HID_BOOT_KEY_COUNT 6

#define HID_MODIFIER_LEFT_CTRL 0
#define HID_MODIFIER_LEFT_SHIFT 1
#define HID_MODIFIER_LEFT_ALT 2
#define HID_MODIFIER_LEFT_GUI 3
#define HID_MODIFIER_RIGHT_CTRL 4
#define HID_MODIFIER_RIGHT_SHIFT 5
#define HID_MODIFIER_RIGHT_ALT 6
#define HID_MODIFIER_RIGHT_GUI 7

typedef struct
{
    KEY_CODE KeyCode;
    uint8_t LeftBit;
    uint8_t RightBit;
} HID_MODIFIER_BINDING;

static KEYBOARD_EVENT KeyboardBuffer[KEYBOARD_BUFFER_SIZE];
static uint32_t BufferHead = 0;
static uint32_t BufferTail = 0;
static uint8_t PreviousModifierMask = 0;
static uint8_t PreviousUsages[HID_BOOT_KEY_COUNT] = {0};

/**
 * 清空键盘输入环形队列。
 */
static void ClearKeyboardBuffer(void);

/**
 * 清空上一次 HID 报告留下的按键状态。
 */
static void ClearHIDState(void);

/**
 * 把一个按键事件塞入环形队列。
 */
static bool PushKeyboardEvent(KEY_CODE key_code, bool pressed);

/**
 * 判断某个 HID Usage 是否出现在 6 键数组中。
 */
static bool IsUsageInReport(uint8_t usage, const uint8_t *usages);

/**
 * 判断指定 HID 修饰键位是否被按下。
 */
static bool IsModifierActive(uint8_t modifier_mask, uint8_t left_bit, uint8_t right_bit);

/**
 * 把 USB HID Keyboard Usage ID 映射成 EvOS 统一键码。
 */
static KEY_CODE GetKeyCodeFromHIDUsage(uint8_t usage);

/**
 * 保存本次 HID 报告里的普通按键数组。
 */
static void CopyKeyboardUsages(const uint8_t *report);

void InitKeyboard(void)
{
    ClearKeyboardBuffer();
    ClearHIDState();
}

void SubmitKeyboardEvent(KEY_CODE key_code, bool pressed)
{
    (void)PushKeyboardEvent(key_code, pressed);
}

bool SubmitKeyboardHIDReport(const uint8_t *report, uint32_t size)
{
    if (report == NULL || size < HID_BOOT_REPORT_SIZE)
        return false;

    uint8_t modifierMask = report[0];
    const uint8_t *currentUsages = report + 2;

    static const HID_MODIFIER_BINDING ModifierBindings[] = {
        {KEY_CTRL, HID_MODIFIER_LEFT_CTRL, HID_MODIFIER_RIGHT_CTRL},
        {KEY_SHIFT, HID_MODIFIER_LEFT_SHIFT, HID_MODIFIER_RIGHT_SHIFT},
        {KEY_ALT, HID_MODIFIER_LEFT_ALT, HID_MODIFIER_RIGHT_ALT},
        {KEY_WIN, HID_MODIFIER_LEFT_GUI, HID_MODIFIER_RIGHT_GUI},
    };

    for (uint32_t index = 0; index < sizeof(ModifierBindings) / sizeof(ModifierBindings[0]); index++)
    {
        bool wasPressed = IsModifierActive(
            PreviousModifierMask,
            ModifierBindings[index].LeftBit,
            ModifierBindings[index].RightBit);
        bool isPressed = IsModifierActive(
            modifierMask,
            ModifierBindings[index].LeftBit,
            ModifierBindings[index].RightBit);

        if (wasPressed != isPressed)
            PushKeyboardEvent(ModifierBindings[index].KeyCode, isPressed);
    }

    for (uint32_t index = 0; index < HID_BOOT_KEY_COUNT; index++)
    {
        uint8_t usage = PreviousUsages[index];
        if (usage == 0 || IsUsageInReport(usage, currentUsages))
            continue;

        PushKeyboardEvent(GetKeyCodeFromHIDUsage(usage), false);
    }

    for (uint32_t index = 0; index < HID_BOOT_KEY_COUNT; index++)
    {
        uint8_t usage = currentUsages[index];
        if (usage == 0 || usage <= 0x03 || IsUsageInReport(usage, PreviousUsages))
            continue;

        PushKeyboardEvent(GetKeyCodeFromHIDUsage(usage), true);
    }

    PreviousModifierMask = modifierMask;
    CopyKeyboardUsages(report);
    return true;
}

bool ReadKeyboard(KEYBOARD_EVENT *event)
{
    if (event == NULL)
        return false;

    if (BufferHead == BufferTail)
        return false;

    *event = KeyboardBuffer[BufferTail];
    BufferTail = (BufferTail + 1) % KEYBOARD_BUFFER_SIZE;

    return true;
}

static void ClearKeyboardBuffer(void)
{
    for (uint32_t index = 0; index < KEYBOARD_BUFFER_SIZE; index++)
    {
        KeyboardBuffer[index].KeyCode = KEY_NONE;
        KeyboardBuffer[index].Pressed = false;
    }

    BufferHead = 0;
    BufferTail = 0;
}

static void ClearHIDState(void)
{
    PreviousModifierMask = 0;
    memset(PreviousUsages, 0, sizeof(PreviousUsages));
}

static bool PushKeyboardEvent(KEY_CODE key_code, bool pressed)
{
    if (key_code == KEY_NONE || key_code >= KEY_MAX_COUNT)
        return false;

    uint32_t nextHead = (BufferHead + 1) % KEYBOARD_BUFFER_SIZE;
    if (nextHead == BufferTail)
        return false;

    KeyboardBuffer[BufferHead].KeyCode = key_code;
    KeyboardBuffer[BufferHead].Pressed = pressed;
    BufferHead = nextHead;
    return true;
}

static bool IsUsageInReport(uint8_t usage, const uint8_t *usages)
{
    if (usages == NULL)
        return false;

    for (uint32_t index = 0; index < HID_BOOT_KEY_COUNT; index++)
        if (usages[index] == usage)
            return true;

    return false;
}

static bool IsModifierActive(uint8_t modifier_mask, uint8_t left_bit, uint8_t right_bit)
{
    uint8_t mask = (uint8_t)((1u << left_bit) | (1u << right_bit));
    return (modifier_mask & mask) != 0;
}

static KEY_CODE GetKeyCodeFromHIDUsage(uint8_t usage)
{
    if (usage >= 0x04 && usage <= 0x1D)
        return (KEY_CODE)(KEY_A + usage - 0x04);

    switch (usage)
    {
    case 0x1E:
        return KEY_1;
    case 0x1F:
        return KEY_2;
    case 0x20:
        return KEY_3;
    case 0x21:
        return KEY_4;
    case 0x22:
        return KEY_5;
    case 0x23:
        return KEY_6;
    case 0x24:
        return KEY_7;
    case 0x25:
        return KEY_8;
    case 0x26:
        return KEY_9;
    case 0x27:
        return KEY_0;
    case 0x28:
        return KEY_ENTER;
    case 0x29:
        return KEY_ESC;
    case 0x2A:
        return KEY_BACKSPACE;
    case 0x2B:
        return KEY_TAB;
    case 0x2C:
        return KEY_SPACE;
    case 0x2D:
        return KEY_MINUS;
    case 0x2E:
        return KEY_EQUAL;
    case 0x2F:
        return KEY_LBRACKET;
    case 0x30:
        return KEY_RBRACKET;
    case 0x31:
        return KEY_BACKSLASH;
    case 0x33:
        return KEY_SEMICOLON;
    case 0x34:
        return KEY_QUOTE;
    case 0x35:
        return KEY_GRAVE;
    case 0x36:
        return KEY_COMMA;
    case 0x37:
        return KEY_PERIOD;
    case 0x38:
        return KEY_SLASH;
    case 0x39:
        return KEY_CAPS_LOCK;
    case 0x3A:
        return KEY_F1;
    case 0x3B:
        return KEY_F2;
    case 0x3C:
        return KEY_F3;
    case 0x3D:
        return KEY_F4;
    case 0x3E:
        return KEY_F5;
    case 0x3F:
        return KEY_F6;
    case 0x40:
        return KEY_F7;
    case 0x41:
        return KEY_F8;
    case 0x42:
        return KEY_F9;
    case 0x43:
        return KEY_F10;
    case 0x44:
        return KEY_F11;
    case 0x45:
        return KEY_F12;
    case 0x4C:
        return KEY_DELETE;
    case 0x4F:
        return KEY_RIGHT;
    case 0x50:
        return KEY_LEFT;
    case 0x51:
        return KEY_DOWN;
    case 0x52:
        return KEY_UP;
    default:
        return KEY_NONE;
    }
}

static void CopyKeyboardUsages(const uint8_t *report)
{
    for (uint32_t index = 0; index < HID_BOOT_KEY_COUNT; index++)
        PreviousUsages[index] = report[index + 2];
}
