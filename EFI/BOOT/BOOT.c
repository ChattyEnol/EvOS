#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
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
static EFI_GUID gEfiDevicePathProtocolGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID gEfiAcpi10TableGuid = ACPI_10_TABLE_GUID;
static EFI_GUID gEfiAcpi20TableGuid = EFI_ACPI_20_TABLE_GUID;

static VOID GetGraphics(VOID);
static VOID GetAcpiRoot();

static BOOLEAN IsGuidEqual(const EFI_GUID *left, const EFI_GUID *right);

static inline UINT16 Rd16(const UINT8 *p)
{
    return (UINT16)(p[0] | ((UINT16)p[1] << 8));
}

static inline UINT32 Rd32(const UINT8 *p)
{
    return (UINT32)p[0] | ((UINT32)p[1] << 8) |
           ((UINT32)p[2] << 16) | ((UINT32)p[3] << 24);
}

// 求一条 device path 的总长（含 End 节点）。
static UINTN GetPathSize(EFI_DEVICE_PATH_PROTOCOL *dp)
{
    UINT8 *p = (UINT8 *)dp;
    UINTN size = 0;
    for (;;)
    {
        UINTN len = (UINTN)p[2] | ((UINTN)p[3] << 8); // NodeLength
        if (p[0] == 0x7F && p[1] == 0xFF)             // End 节点
            return size + len;
        if (len < 4) // 防野指针，坏路径到此为止。
            return size;
        size += len;
        p += len;
    }
}

// 设备前缀 + 文件路径节点 + End = 完整路径。
static EFI_DEVICE_PATH_PROTOCOL *BuildKernelPath(EFI_HANDLE device, CHAR16 *path)
{
    EFI_DEVICE_PATH_PROTOCOL *prefix = NULL;
    EV_LOAD(BOOT_SERVICES->HandleProtocol(
        device, &gEfiDevicePathProtocolGuid, (VOID **)&prefix));

    UINTN prefixSize = GetPathSize(prefix); // 含 End。
    UINTN pathLen = 0;
    while (path[pathLen] != L'\0')
        pathLen++;
    UINTN fileNodeLen = 4 + (pathLen + 1) * 2;        // 节点头 4B + UTF-16 串(含 L'\0')。
    UINTN total = (prefixSize - 4) + fileNodeLen + 4; // 前缀(去 End) + 文件节点 + End。

    EFI_DEVICE_PATH_PROTOCOL *full = NULL;
    EV_LOAD(BOOT_SERVICES->AllocatePool(EfiLoaderData, total, (VOID **)&full));

    UINT8 *dst = (UINT8 *)full;
    UINT8 *src = (UINT8 *)prefix;
    for (UINTN i = 0; i < prefixSize - 4; i++) // 抄设备前缀，丢掉它自带的 End。
        dst[i] = src[i];
    dst += prefixSize - 4;

    dst[0] = 0x04; // Type = MEDIA_DEVICE_PATH
    dst[1] = 0x04; // SubType = MEDIA_FILEPATH_DP
    dst[2] = (UINT8)(fileNodeLen & 0xFF);
    dst[3] = (UINT8)(fileNodeLen >> 8);
    for (UINTN i = 0; i <= pathLen; i++) // 0..pathLen，含结尾 L'\0'。
    {
        dst[4 + 2 * i] = (UINT8)(path[i] & 0xFF);
        dst[4 + 2 * i + 1] = (UINT8)(path[i] >> 8);
    }
    dst += fileNodeLen;

    dst[0] = 0x7F; // End 节点
    dst[1] = 0xFF;
    dst[2] = 0x04;
    dst[3] = 0x00;
    return full;
}

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
    EV_LOAD(BOOT_SERVICES->HandleProtocol( // 定位现在这个 EFI 应用。
        ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&loadedImage));
    EFI_HANDLE device = loadedImage->DeviceHandle; // 现在这个 EFI 应用所在的设备。
    loadedImage = NULL;                            // 后面我复用了，现在置空。

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *volume = NULL;
    EV_LOAD(BOOT_SERVICES->HandleProtocol( // 获取现在这个 EFI 所在设备的卷。
        device, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&volume));
    EFI_FILE_PROTOCOL *root = NULL;             // 根目录。
    EV_LOAD(volume->OpenVolume(volume, &root)); // 打开卷的根目录。
    volume = NULL;                              // 卷：你的任务结束了。

    // 读文件。
    EFI_FILE_PROTOCOL *kernelFile = NULL;
    EV_LOAD(root->Open(                                    // 找到我亲爱的内核。
        root, &kernelFile, L"\\EFI\\Evoncil\\Evoncil.EFI", // 这里不能用 `/`。
        EFI_FILE_MODE_READ, 0));

    // [fileSize] 跳到最后再跳回来。
    EV_LOAD(kernelFile->SetPosition(kernelFile, 0xFFFFFFFFFFFFFFFF));
    UINTN fileSize = 0;
    EV_LOAD(kernelFile->GetPosition(kernelFile, &fileSize));
    EV_LOAD(kernelFile->SetPosition(kernelFile, 0));
    UINTN pages = (fileSize + 0xFFF) / 0x1000; // 算我要几个页（一个页 4KB）。

    // [kernelBuffer] 加载到内存里。
    EFI_PHYSICAL_ADDRESS kernelBuffer = 0;
    EV_LOAD(BOOT_SERVICES->AllocatePages(
        AllocateAnyPages, EfiLoaderData, pages, &kernelBuffer));
    EV_LOAD(kernelFile->Read(kernelFile, &fileSize, (VOID *)kernelBuffer));
    // 用完就关。
    kernelFile->Close(kernelFile);
    kernelFile = NULL;
    root->Close(root);
    root = NULL;

    // [kernelHandle] 准备接收输出参数。
    EFI_HANDLE kernelHandle = ImageHandle;
    EFI_DEVICE_PATH_PROTOCOL *kpath =
        BuildKernelPath(device, L"\\EFI\\Evoncil\\Evoncil.EFI");
    EV_LOAD(BOOT_SERVICES->LoadImage( // 更新后的 Hyper-V 俩可选参数都要！真是花心啊！调试了我整整半年！
        FALSE, ImageHandle, kpath, (VOID *)kernelBuffer, fileSize, &kernelHandle));

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