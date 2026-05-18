#include <EvLoader.h>
#include <World/World.h>

EFI_STATUS EV_LOADER_STATUS = EFI_SUCCESS;

// 初始化库函数用的，我也尽量用我的全局变量吧。

EFI_SYSTEM_TABLE *SYSTEM_TABLE = NULL;
EFI_BOOT_SERVICES *BOOT_SERVICES = NULL;

void Initialize(EFI_SYSTEM_TABLE *SystemTable)
{
    SYSTEM_TABLE = SystemTable;
    BOOT_SERVICES = SystemTable->BootServices;
}

void Deinitialize(void)
{
    SYSTEM_TABLE = NULL;
    BOOT_SERVICES = NULL;
}

// 给 Evoncil 的全世界。
WORLD World = {0};