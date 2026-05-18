#include <EvLoader.h>
#include <Options.h>
#include <World/World.h>
#include <stdio.h>

int Opt(MENU_OPTION choice)
{
    if (choice == MENU_FIRMWARE)
    {
        printf("\r\n[ Firmware Info ]\r\n");
        printf("\r\nVendor:\r\n");
        EV_LOAD(SYSTEM_TABLE->ConOut->OutputString(
            SYSTEM_TABLE->ConOut, SYSTEM_TABLE->FirmwareVendor));
        printf("\r\n");
    }
    else if (choice == MENU_GRAPHICS)
    {
        printf("\r\n[ Graphics Info ]\r\n");
        printf("\r\nFrameBuffer Base: 0x%p\r\n", World.Graphics.FrameBufferBase);
        printf("FrameBuffer Size: %d Bytes\r\n", (UINT32)World.Graphics.FrameBufferSize);
        printf("Res: %d x %d\r\n",
               World.Graphics.HorizontalResolution,
               World.Graphics.VerticalResolution);
        printf("PixelsPerScanLine: %d\r\n", World.Graphics.PixelsPerScanLine);
    }
    else if (choice == MENU_BOOT)
    {
        printf("Booting EvOS...\r\n");
        return 0;
    }
    else
        printf("Invalid option.\r\nHow did you get into this page?\r\n");

    printf("\r\nPress [ESC] to return to menu...\r\n");
    EFI_INPUT_KEY inputKey;
    do
    {
        UINTN index;
        EV_LOAD(BOOT_SERVICES->WaitForEvent(1, &SYSTEM_TABLE->ConIn->WaitForKey, &index));
        EV_LOAD(SYSTEM_TABLE->ConIn->ReadKeyStroke(SYSTEM_TABLE->ConIn, &inputKey));
    } while (inputKey.ScanCode != SCAN_ESC);

    return 1; // 继续菜单。
}