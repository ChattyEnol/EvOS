#include <Uefi.h>
#include <Protocol/LoadedImage.h>

#include <World/World.h>
#include <EvStub.h>

EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

EFI_STATUS EFIAPI EvStub(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_LOADED_IMAGE_PROTOCOL *loadedImage;
    SystemTable->BootServices->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&loadedImage);

    // 世界指针。
    world = (WORLD *)loadedImage->LoadOptions;
    // 内存信息。
    // 这是第一次试探。第一次的目的往往很单纯，我们只需要知道内存信息大概有多大。
    // 比如这里只是为了获取 `memoryMapSize`。
    // 告诉 UEFI 我是 0，`MemoryMapSize` 是个 IN OUT 类型的参数。
    world->Memory.MemoryMapSize = 0;
    // 它会报 BUFFER_TOO_SMALL，UEFI 不喜欢 0。
    SystemTable->BootServices->GetMemoryMap(
        &world->Memory.MemoryMapSize, // 第一次试探，我们来当 0。
        NULL,                         // 不能真放了，真放了就真变 0 了。
        /* 下面的第一次暂时不管。 */
        &world->Memory.MapKey,
        &world->Memory.DescriptorSize,
        &world->Memory.DescriptorVersion);
    // 完事后 UEFI 会告诉你他喜欢多大的。
    // 想不到吧！下一步会改变内存布局。我们需要变大，不变大就会被撑破。
    world->Memory.MemoryMapSize += world->Memory.DescriptorSize * 8;
    // 我们已经够大了，现在需要找一个地方塞下我们这个大大的东西。
    // 这个 `AllocatePool` 会给我他给我给我分配的 `&World.Memory.MemoryMap`。
    SystemTable->BootServices->AllocatePool(
        2, world->Memory.MemoryMapSize, (VOID **)&world->Memory.MemoryMap);
    // 现在我们不是第一次了。
    // 这一次，我们就坦诚相见吧！
    SystemTable->BootServices->GetMemoryMap(
        // 顾名思义，内存图的大小。
        &world->Memory.MemoryMapSize,
        // 存内存图的地址。
        world->Memory.MemoryMap, // 直接给指针。我们已经申请了合法指针，UEFI 直接进入！
        // 类似于内存信息版本的新旧程度？退出启动服务时要提供这个参数。
        &world->Memory.MapKey,
        // 一个内存区域的描述符有多大。
        &world->Memory.DescriptorSize,
        // 版本号，不知道干啥的。
        &world->Memory.DescriptorVersion);
    // 退。
    SystemTable->BootServices->ExitBootServices(ImageHandle, world->Memory.MapKey);
    // 走！
    Evoncil(world);
    return EFI_SUCCESS;
}
