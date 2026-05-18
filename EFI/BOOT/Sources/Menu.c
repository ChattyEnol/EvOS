#include <EvLoader.h>
#include <stdio.h>
#include <Menu.h>

static const char *LOGO[] = {
    "  ______                           _   _      ____    _____ \r\n",
    " |  ____|                         (_) | |    / __ \\  /  ___|\r\n",
    " | |__  __   __ ___  _ __    ___   _  | |   | |  | | | (___  \r\n",
    " |  __| \\ \\ / // _ \\| '_ \\  / __| | | | |   | |  | |  \\___ \\ \r\n",
    " | |____ \\ V /| (_) | | | || (__  | | | |   | |__| |  ____) |\r\n",
    " |______| \\_/  \\___/|_| |_| \\___| |_| |_|    \\____/  |_____/ \r\n",
    "                                                              \r\n",
    "--------------------------------------------------------------\r\n",
    "                                                              \r\n",
    "                           By Enol                            \r\n"};

static const char *TITLE[] = {
    "[ Firmware Info ]",
    "[ Memory Info ]",
    "[ Graphics Info ]",
    "[ Boot EvOS ]"};

static const char *Title(MENU_OPTION option)
{
    switch (option)
    {
    case MENU_FIRMWARE:
        return "[ Firmware Info ]";
    case MENU_GRAPHICS:
        return "[ Graphics Info ]";
    case MENU_BOOT:
        return "[ Boot EvOS ]";
    default:
        return "[ Unknown ]";
    }
}

MENU_OPTION Menu()
{
    int currentSelection = 0;
    EFI_INPUT_KEY inputKey;
    // 清空输入缓存。
    EV_LOAD(SYSTEM_TABLE->ConIn->Reset(SYSTEM_TABLE->ConIn, FALSE));

    while (1)
    {
        EV_LOAD(SYSTEM_TABLE->ConOut->SetAttribute(
            SYSTEM_TABLE->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK));
        EV_LOAD(SYSTEM_TABLE->ConOut->ClearScreen(SYSTEM_TABLE->ConOut));
        for (int i = 0; i < 10; i++)
            printf("%s", LOGO[i]);
        printf("\r\n");

        // Options
        for (int optionIndex = 0; optionIndex < MENU_MAX; optionIndex++)
        {
            if (optionIndex == currentSelection)
                EV_LOAD(SYSTEM_TABLE->ConOut->SetAttribute(
                    SYSTEM_TABLE->ConOut, EFI_BLACK | EFI_BACKGROUND_CYAN));
            else
                EV_LOAD(SYSTEM_TABLE->ConOut->SetAttribute(
                    SYSTEM_TABLE->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK));
            printf(" %d. %s \r\n\r\n", optionIndex + 1, Title(optionIndex));
            EV_LOAD(SYSTEM_TABLE->ConOut->SetAttribute(
                SYSTEM_TABLE->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK));
        }

        // 阻塞等待用户输入。
        UINTN index;
        EV_LOAD(BOOT_SERVICES->WaitForEvent(1, &SYSTEM_TABLE->ConIn->WaitForKey, &index));
        EV_LOAD(SYSTEM_TABLE->ConIn->ReadKeyStroke(SYSTEM_TABLE->ConIn, &inputKey));

        if (inputKey.ScanCode == SCAN_UP) // Up 0x01
            currentSelection = (currentSelection > 0) ? currentSelection - 1 : MENU_MAX - 1;
        else if (inputKey.ScanCode == SCAN_DOWN) // Down 0x02
            currentSelection = (currentSelection < MENU_MAX - 1) ? currentSelection + 1 : 0;
        else if (inputKey.UnicodeChar == '\r') // Enter (0x0D?)
        {
            EV_LOAD(SYSTEM_TABLE->ConOut->ClearScreen(SYSTEM_TABLE->ConOut));
            return (MENU_OPTION)currentSelection;
        }
    }
}
