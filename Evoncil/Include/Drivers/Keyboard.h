/** Drivers/Keyboard.h
 *
 * (C) Charity Enol
 *
 * 现代键盘输入层头文件。
 * 这里不再碰 PS/2，也不再读 0x60/0x64 这种老硬件端口。
 * xHCI / USB HID 驱动只需要把标准 HID Boot Keyboard Report 喂进来，
 * 这个模块负责转换成 EvOS 自己统一的 KEY_CODE。
 * 字符、大小写、快捷键这些仍然交给 TextIO 和更上层模块处理。
 */

#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

/**
 * 平台无关的 Key Code 枚举。
 * 对应键盘上的每一个坑位，哪怕它没有 ASCII 码。
 * 这只是按键，不对应 ASCII 码，不对应真实字符！
 */
typedef enum
{
    KEY_NONE = 0,

    // 第一排：功能键。
    KEY_ESC,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_DELETE,

    // 第二排：数字与主符号。
    KEY_GRAVE,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_BACKSPACE,

    // 主字母区与周边符号。
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_LBRACKET,  // [ {
    KEY_RBRACKET,  // ] }
    KEY_BACKSLASH, // \ |
    KEY_SEMICOLON, // ; :
    KEY_QUOTE,     // ' "
    KEY_ENTER,     // 回车键
    KEY_COMMA,     // , <
    KEY_PERIOD,    // . >
    KEY_SLASH,     // / ?

    // 控制与修饰键，不想区分左右。
    KEY_TAB,
    KEY_CAPS_LOCK,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_FN,
    KEY_WIN,
    KEY_ALT,
    KEY_SPACE,

    // 方向键。
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,

    // 状态保持标记，用来做边界检查。
    KEY_MAX_COUNT
} KEY_CODE;

// 键盘按键事件。
typedef struct
{
    KEY_CODE KeyCode; // 转换后的键码。
    bool Pressed;     // true = 按, false = 弹起。
} KEYBOARD_EVENT;

/**
 * 初始化键盘驱动。
 * 只清空软件输入队列和 HID 状态，不做任何硬件初始化。
 */
void InitKeyboard(void);

/**
 * 向统一键盘队列提交一个已经转换好的按键事件。
 * 这个接口主要留给未来更复杂的 HID 解析器或虚拟输入设备使用。
 */
void SubmitKeyboardEvent(KEY_CODE key_code, bool pressed);

/**
 * 从键盘缓冲区中异步读取一个按键事件
 * @param [out] event 指向接收事件的结构体指针
 * @return bool 如果成功读到事件返回 true；如果缓冲区为空返回 false
 */
bool ReadKeyboard(KEYBOARD_EVENT *event);

#endif // DRIVERS_KEYBOARD_H
