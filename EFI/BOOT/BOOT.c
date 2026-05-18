#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>

#include <EvLoader.h>
#include <World/World.h>
#include <Menu.h>
#include <Options.h>
#include <stdio.h>

/**
 * 这些长长地 GUID 似乎在 EDK II 的头文件里有外部声明。
 * 那我就勉为其难地用吧。
 * 不过确实长得好丑啊……
 */

static EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID gEfiAcpi10TableGuid = ACPI_10_TABLE_GUID;
static EFI_GUID gEfiAcpi20TableGuid = EFI_ACPI_20_TABLE_GUID;

static VOID GetGraphics(VOID);
static VOID GetAcpiRoot();

static BOOLEAN IsGuidEqual(const EFI_GUID *left, const EFI_GUID *right);

// `BOOTX64.EFI` 的入口。
EFI_STATUS EFIAPI EvLoader(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    /**
     * 怎么说呢。
     * UEFI 的大部分功能都需要 SystemTable。
     * 所以还是定义了一个全局变量，然后统一初始化。
     */

    Initialize(SystemTable);
    if (SYSTEM_TABLE == NULL)
        return -1;

    /**
     * 本来应该收集要传给 Evoncil 的信息，然后我会把这些信息放在 `WORLD` 结构体里。
     * 我现在只关心内存和显卡。但等下啊，现在收集内存信息没用。
     * 内存信息应该在 `ExitBootServices` 之前收集。
     * 因为这个函数需要一个 MapKey 参数来看我们的内存信息是不是最新的。
     * 那现在就只收集显示信息吧。
     */

    GetGraphics();
    GetAcpiRoot();

    // 菜单只在选择“Boot EvOS”的时候退出。
    while (1)
        if (!Opt(Menu()))
            break;

    /**
     * 现在启动 Evoncil。
     * 我把 OS 做成了 UEFI Stub，所以就像加载 EFI 应用一样加载它吧。
     */

    EFI_LOADED_IMAGE_PROTOCOL *loadedImage;
    BOOT_SERVICES->HandleProtocol(
        ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&loadedImage);
    // 现在这个 EFI 应用所在的设备。
    EFI_HANDLE device = loadedImage->DeviceHandle;
    // 后面我复用了，现在置空。
    loadedImage = NULL;
    
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *volume = NULL;
    BOOT_SERVICES->HandleProtocol(
        device, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&volume);
    // 根目录。
    EFI_FILE_PROTOCOL *root = NULL;
    EV_LOAD(volume->OpenVolume(volume, &root));
    volume = NULL;
    // 读文件。
    EFI_FILE_PROTOCOL *kernelFile = NULL;
    EV_LOAD(root->Open( // 真奇怪，不能用 `/`。
        root, &kernelFile, L"\\EFI\\Evoncil\\Evoncil.EFI", EFI_FILE_MODE_READ, 0));
    // 获取大小：跳到最后再跳回来。
    kernelFile->SetPosition(kernelFile, 0xFFFFFFFFFFFFFFFF);
    UINT64 fileSize = 0;
    kernelFile->GetPosition(kernelFile, &fileSize);
    kernelFile->SetPosition(kernelFile, 0);
    // 算我要几个页（一个页 4KB）。
    UINTN pages = (fileSize + 0xFFF) / 0x1000;
    // 加载到内存里。
    EFI_PHYSICAL_ADDRESS kernelBuffer = 0;
    EV_LOAD(BOOT_SERVICES->AllocatePages(
        AllocateAnyPages, EfiLoaderData, pages, &kernelBuffer));
    EV_LOAD(kernelFile->Read(kernelFile, &fileSize, (VOID *)kernelBuffer));
    // 用完就关，真不喜欢野指针。
    kernelFile->Close(kernelFile);
    kernelFile = NULL;
    root->Close(root);
    root = NULL;

    EFI_HANDLE kernelHandle = NULL;
    EV_LOAD(BOOT_SERVICES->LoadImage(
        FALSE, ImageHandle, NULL, (VOID *)kernelBuffer, fileSize, &kernelHandle));

    // 传递世界指针。
    EV_LOAD(BOOT_SERVICES->HandleProtocol(
        kernelHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&loadedImage));
    loadedImage->LoadOptions = &World;
    loadedImage->LoadOptionsSize = sizeof(WORLD);
    loadedImage = NULL;

    /**
     * 不喜欢野指针！所以要去初始化。
     * 从此释放全局变量 `SYSTEM_TABLE` 和 `BOOT_SERVICES`。
     * 大部分 LibC 函数都都不能用了。
     */

    Deinitialize();

    // Go go go!
    SystemTable->BootServices->StartImage(kernelHandle, NULL, NULL);
    return EFI_SUCCESS;
}

static VOID GetGraphics(VOID)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP = NULL;
    EV_LOAD(BOOT_SERVICES->HandleProtocol(
        SYSTEM_TABLE->ConsoleOutHandle,
        &gEfiGraphicsOutputProtocolGuid,
        (VOID **)&GOP));

    World.Graphics = (GRAPHICS){
        .FrameBufferBase = GOP->Mode->FrameBufferBase,
        .FrameBufferSize = GOP->Mode->FrameBufferSize,
        .HorizontalResolution = GOP->Mode->Info->HorizontalResolution,
        .VerticalResolution = GOP->Mode->Info->VerticalResolution,
        .PixelsPerScanLine = GOP->Mode->Info->PixelsPerScanLine};
}

static VOID GetAcpiRoot()
{
    VOID *fallbackAcpiRoot = NULL;
    World.AcpiRoot = NULL;

    for (UINTN index = 0; index < SYSTEM_TABLE->NumberOfTableEntries; index++)
    {
        EFI_CONFIGURATION_TABLE *table = &SYSTEM_TABLE->ConfigurationTable[index];

        if (IsGuidEqual(&table->VendorGuid, &gEfiAcpi20TableGuid))
        {
            World.AcpiRoot = table->VendorTable;
            return;
        }

        if (IsGuidEqual(&table->VendorGuid, &gEfiAcpi10TableGuid))
            fallbackAcpiRoot = table->VendorTable;
    }

    World.AcpiRoot = fallbackAcpiRoot;
}

static BOOLEAN IsGuidEqual(const EFI_GUID *left, const EFI_GUID *right)
{
    const UINT8 *leftBytes = (const UINT8 *)left;
    const UINT8 *rightBytes = (const UINT8 *)right;

    for (UINTN index = 0; index < sizeof(EFI_GUID); index++)
        if (leftBytes[index] != rightBytes[index])
            return FALSE;

    return TRUE;
}