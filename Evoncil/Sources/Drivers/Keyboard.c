/** Drivers/Keyboard.c
 *
 * (C) 2026 Charity Enol
 *
 * 核心输入环形队列实现，不含任何总线特化逻辑。
 */

#include <Drivers/Keyboard.h>
#include <stddef.h>

#define KEYBOARD_BUFFER_SIZE 256

static KEYBOARD_EVENT KeyboardBuffer[KEYBOARD_BUFFER_SIZE];
static uint32_t BufferHead = 0;
static uint32_t BufferTail = 0;

void InitKeyboard(void)
{
    BufferHead = 0;
    BufferTail = 0;
}

void SubmitKeyboardEvent(KEY_CODE key_code, bool pressed)
{
    if (key_code == KEY_NONE || key_code >= KEY_MAX_COUNT)
        return;

    uint32_t next_head = (BufferHead + 1) % KEYBOARD_BUFFER_SIZE;

    // 如果队列满了，就暂时丢弃新事件防止缓冲区溢出
    if (next_head == BufferTail)
        return;

    KeyboardBuffer[BufferHead].KeyCode = key_code;
    KeyboardBuffer[BufferHead].Pressed = pressed;
    BufferHead = next_head;
}

bool ReadKeyboard(KEYBOARD_EVENT *event)
{
    if (event == NULL)
        return false;

    // 缓冲区为空，直接返回 false
    if (BufferHead == BufferTail)
        return false;

    *event = KeyboardBuffer[BufferTail];
    BufferTail = (BufferTail + 1) % KEYBOARD_BUFFER_SIZE;

    return true;
}