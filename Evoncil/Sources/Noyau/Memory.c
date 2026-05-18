/** Noyau/Memory.c
 *
 * (C) Charity Enol
 *
 * 物理页用空闲区间表。
 * 虚拟页用线性递增的内核空间。
 */

#include <Noyau/Memory.h>
#include <HAL/HAL.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 0x1000ull             // 4KB，AMD64 下最标准的大小吧应该。
#define PAGE_MASK 0xFFFFFFFFFFFFF000ull // 得到页号的掩码。
#define PAGE_OFFSET 0xFFFull            // 得到页内偏移的掩码。

/** 参考：AMD64 4KB 页表项标志位说明：
 *
 * [0]   0x001  Present（存在位）
 *       =1 表示该页有效；=0 表示缺页，会触发 Page Fault。
 *
 * [1]   0x002  Read/Write（读写位）
 *       =0 只读；=1 可写。
 *
 * [2]   0x004  User/Supervisor（用户/内核）
 *       =0 仅内核可访问；=1 用户态可访问。
 *
 * [3]   0x008  Page-level Write-Through（写穿）
 *       =1 表示写穿缓存策略。
 *
 * [4]   0x010  Page-level Cache Disable（禁用缓存）
 *       =1 表示该页不使用缓存。
 *
 * [5]   0x020  Accessed（访问位）
 *       CPU 自动置 1，表示该页被访问过。
 *
 * [6]   0x040  Dirty（脏位）
 *       CPU 自动置 1，表示该页被写过（仅对 PTE 有意义）。
 *
 * [7]   0x080  Page Size（页大小）
 *       在 PTE 中必须为 0；在 PDE 中 =1 表示 2MB 大页。
 *
 * [8]   0x100  Global（全局页）
 *       =1 表示 TLB 不会因为 CR3 切换而刷新该页。
 *
 * [9]   0x200  Available to software
 * [10]  0x400  Available to software
 * [11]  0x800  Available to software
 *       这三位留给 OS 自己用。
 *
 * [12..51]  物理页框地址（PFN）
 *           必须按 4KB 对齐（低 12 bit 为 0）。
 *
 * [52]  0x0010_0000_0000_0000  Protection Key（PK）
 * [53]  0x0020_0000_0000_0000  Protection Key
 * [54]  0x0040_0000_0000_0000  Protection Key
 * [55]  0x0080_0000_0000_0000  Protection Key
 *       Intel MPK（内存保护键），可选。
 *
 * [56]  0x0100_0000_0000_0000  Available to software
 * [57]  0x0200_0000_0000_0000  Available to software
 * [58]  0x0400_0000_0000_0000  Available to software
 * [59]  0x0800_0000_0000_0000  Available to software
 * [60]  0x1000_0000_0000_0000  Available to software
 * [61]  0x2000_0000_0000_0000  Available to software
 * [62]  0x4000_0000_0000_0000  Available to software
 *
 * [63]  0x8000_0000_0000_0000  No-Execute（NX）
 *       =1 表示该页不可执行（需要 EFER.NXE=1）。
 */

#define PAGE_PRESENT 0x001ull       // [P] 存在位。
#define PAGE_READWRITE 0x002ull     // [R/W] 读写位。0：只读（代码段）；1：可读可写（数据段）。
#define PAGE_WRITETHROUGH 0x008ull  // [PWT] 通写位。开启后直接写物理内存不走缓存。（显存？）
#define PAGE_CACHE_DISABLE 0x010ull // [PCD] 禁用缓存。防止读取到 CPU 缓存里的旧数据。
#define PAGE_LARGE 0x080ull         // [PS] 大页位。开了它一页就是 2MB 而不是 4KB。
#define PAGE_GLOBAL 0x100ull        // [G] 全局位。切换任务时不刷新 TLB 缓存，似乎能提速。

/**
 * 这些是自定义的规模。
 * 取大了怕把我电脑烧了。
 * 等在林子宸电脑上跑之前再改大一点。
 */

#define PAGE_TABLE_ENTRY_COUNT 512           // 每个页表（PML4/PDPT/PD/PT）固定 512 个坑位。
#define MAX_FREE_MEMORY_REGIONS 128          // 我们最多能记录 128 块不连续的空闲内存。
#define LOW_MEMORY_RESERVED_SIZE 0x100000ull // 前 1MB 是固件的某些怪东西，他不欢迎我。

/**
 * 虚拟地址空间大大的地图。
 * 64 位地址很大，我们要给内核和设备划定专属的高位领土。
 * 内核需要在天上，设备在内核下面。
 * AMD64 有个啥 Canonical Address 要求：
 * 高 16 位必须和第 47 位保持一致。
 * 所以要么用 0x0000...（用户态），要么用 0xFFFF...（内核态）。
 */

#define KERNEL_HEAP_START 0xFFFF900000000000ull   // 内核动态申请堆的起点。
#define DEVICE_MEMORY_START 0xFFFFA00000000000ull // 设备的映射，第一个就给显卡吧！

// UEFI 里定义的，初版 EvOS 里可以看，现在删了（）
#define EFI_CONVENTIONAL_MEMORY 7

/** 参考文档：EDK II 的头文件。
  typedef struct {
  ///
  /// Type of the memory region.
  /// Type EFI_MEMORY_TYPE is defined in the
  /// AllocatePages() function description.
  ///
  UINT32                  Type;
  ///
  /// Physical address of the first byte in the memory region. PhysicalStart must be
  /// aligned on a 4 KiB boundary, and must not be above 0xfffffffffffff000. Type
  /// EFI_PHYSICAL_ADDRESS is defined in the AllocatePages() function description
  ///
  EFI_PHYSICAL_ADDRESS    PhysicalStart;
  ///
  /// Virtual address of the first byte in the memory region.
  /// VirtualStart must be aligned on a 4 KiB boundary,
  /// and must not be above 0xfffffffffffff000.
  ///
  EFI_VIRTUAL_ADDRESS     VirtualStart;
  ///
  /// NumberOfPagesNumber of 4 KiB pages in the memory region.
  /// NumberOfPages must not be 0, and must not be any value
  /// that would represent a memory page with a start address,
  /// either physical or virtual, above 0xfffffffffffff000.
  ///
  UINT64                  NumberOfPages;
  ///
  /// Attributes of the memory region that describe the bit mask of capabilities
  /// for that memory region, and not necessarily the current settings for that
  /// memory region.
  ///
  UINT64                  Attribute;
} EFI_MEMORY_DESCRIPTOR;
 */

typedef struct
{
    uint32_t Type;    // 只认 7。
    uint32_t Padding; // 这啥玩意儿？不加还对不齐。非要 8B 对齐吗？
    uint64_t PhysicalStart;
    uint64_t VirtualStart; // UEFI 曾经给它映射过的虚拟位置（进入内核后咱就不理它了）。
    uint64_t NumberOfPages;
    uint64_t Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef struct
{
    uint64_t baseAddress;
    uint64_t pageCount;
} FREE_MEMORY_REGION;

// 页表的根。这个指针指向内核自己的 PML4 表的物理地址。
static uint64_t *PAGE_MAP_LEVEL_4 = NULL;
static uint64_t FREE_MEMORY_SIZE = 0;
static uint64_t USED_MEMORY_SIZE = 0;
// 内核堆的分配指针（线性递增）。
static uint64_t NEXT_KERNEL_VIRTUAL_ADDRESS = KERNEL_HEAP_START;
// 设备内存的映射指针（线性递增）。
static uint64_t NEXT_DEVICE_VIRTUAL_ADDRESS = DEVICE_MEMORY_START;
// 当前有多少块不连续的空闲物理内存区间。
static uint64_t FREE_MEMORY_REGION_COUNT = 0;
static FREE_MEMORY_REGION FREE_MEMORY_REGIONS[MAX_FREE_MEMORY_REGIONS];

/**
 * 字节对齐的小工具。
 */

static uint64_t AlignUp(uint64_t value, uint64_t alignment);   // 向上对齐到最近的倍数。
static uint64_t AlignDown(uint64_t value, uint64_t alignment); // 向下对齐到最近的倍数。

/**
 * 物理页框管理。
 * 这些管理的是空闲的物理内存。
 */

// 登记一块新的空闲物理页。
static void AddFreePhysicalPages(uint64_t base_address, uint64_t page_count);
// 从空闲区间表里删除某块区域。
static void RemoveFreeMemoryRegion(uint64_t region_index);
// 把相邻的空闲块合并成一块（碎片整理）。
static void MergeFreeMemoryRegions(void);
// 从空闲物理页里拿一页出来。
static void *AllocatePhysicalPage(void);
// 把物理页还回去。
static void FreePhysicalPage(void *address);

/**
 * 页表操作。一共有四层页表。
 * 64 位地址需要 PML4 -> PDPT -> PD -> PT 四层页表才能把虚拟转成物理。
 */

// 获取某层页表的条目，没有就创建。
static uint64_t *GetOrCreatePageTable(uint64_t *table, uint64_t index, uint64_t flags);
// 只读访问页表条目，没有就返回 NULL。
static uint64_t *GetPageTable(uint64_t *table, uint64_t index);

/**
 * 虚拟地址映射。
 */

// 把一个虚拟页连接到物理页。
static void MapSinglePage(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
// 断开虚拟页的连接，返回它的物理地址。
static uint64_t UnmapSinglePage(uint64_t virtual_address);
// 根据虚拟地址查询它的物理地址。
static uint64_t GetMappedPhysicalAddress(uint64_t virtual_address);

void InitMemory(MEMORY *memory)
{
    FREE_MEMORY_SIZE = 0;
    USED_MEMORY_SIZE = 0;
    FREE_MEMORY_REGION_COUNT = 0;
    NEXT_KERNEL_VIRTUAL_ADDRESS = KERNEL_HEAP_START;
    NEXT_DEVICE_VIRTUAL_ADDRESS = DEVICE_MEMORY_START;

    /**
     * 下面开始遍历 UEFI 的内存图。
     * 可用的（不在保留区）登记在 `FREE_MEMORY_REGION`。
     */

    uint8_t *memoryMap = (uint8_t *)memory->MemoryMap;
    for (uint64_t offset = 0; offset < memory->MemoryMapSize; offset += memory->DescriptorSize)
    {
        // offset 的步长是 DescriptorSize，所以直接加就好了。
        EFI_MEMORY_DESCRIPTOR *descriptor = (EFI_MEMORY_DESCRIPTOR *)(memoryMap + offset);
        uint64_t baseAddress = descriptor->PhysicalStart;
        uint64_t endAddress = descriptor->PhysicalStart + descriptor->NumberOfPages * PAGE_SIZE;

        if (descriptor->Type != EFI_CONVENTIONAL_MEMORY || // 不可用的。
            endAddress <= LOW_MEMORY_RESERVED_SIZE)        // 低地址一般保留，怕空指针吧？
        {
            USED_MEMORY_SIZE += descriptor->NumberOfPages * PAGE_SIZE;
            continue;
        }

        if (baseAddress < LOW_MEMORY_RESERVED_SIZE)
            baseAddress = LOW_MEMORY_RESERVED_SIZE;

        baseAddress = AlignUp(baseAddress, PAGE_SIZE);
        endAddress = AlignDown(endAddress, PAGE_SIZE);
        if (endAddress <= baseAddress)
            continue;

        AddFreePhysicalPages(baseAddress, (endAddress - baseAddress) / PAGE_SIZE);
    }

    MergeFreeMemoryRegions();

    uint64_t *oldPageMapLevel4 = (uint64_t *)GetPageTableRoot();

    PAGE_MAP_LEVEL_4 = AllocatePhysicalPage();
    if (PAGE_MAP_LEVEL_4 == NULL)
        return;

    // 先继承当前映射。刚离开 UEFI 的时候，内核、栈、显存都还靠它活着呢。
    memcpy(PAGE_MAP_LEVEL_4, oldPageMapLevel4, PAGE_SIZE);
    SetPageTableRoot((uint64_t)PAGE_MAP_LEVEL_4);
}

void *MapDeviceMemory(uint64_t physical_address, uint64_t size)
{
    if (size == 0)
        return NULL;

    uint64_t physicalBase = AlignDown(physical_address, PAGE_SIZE);
    uint64_t offset = physical_address - physicalBase;
    uint64_t mapSize = AlignUp(size + offset, PAGE_SIZE);
    uint64_t pageCount = mapSize / PAGE_SIZE;
    uint64_t virtualBase = NEXT_DEVICE_VIRTUAL_ADDRESS;

    NEXT_DEVICE_VIRTUAL_ADDRESS += mapSize;

    for (uint64_t index = 0; index < pageCount; index++)
    {
        MapSinglePage(
            virtualBase + index * PAGE_SIZE,
            physicalBase + index * PAGE_SIZE,
            PAGE_PRESENT | PAGE_READWRITE | PAGE_CACHE_DISABLE | PAGE_GLOBAL);
    }

    return (void *)(virtualBase + offset);
}

void *AllocatePage(uint64_t size)
{
    if (size == 0)
        return NULL;

    uint64_t pageCount = AlignUp(size, PAGE_SIZE) / PAGE_SIZE;
    uint64_t virtualBase = NEXT_KERNEL_VIRTUAL_ADDRESS;

    NEXT_KERNEL_VIRTUAL_ADDRESS += pageCount * PAGE_SIZE;

    for (uint64_t index = 0; index < pageCount; index++)
    {
        void *physicalPage = AllocatePhysicalPage();
        if (physicalPage == NULL)
        {
            FreePage((void *)virtualBase, index * PAGE_SIZE);
            return NULL;
        }

        memset(physicalPage, 0, PAGE_SIZE);
        MapSinglePage(
            virtualBase + index * PAGE_SIZE,
            (uint64_t)physicalPage,
            PAGE_PRESENT | PAGE_READWRITE);
    }

    return (void *)virtualBase;
}

void FreePage(void *address, uint64_t size)
{
    if (address == NULL || size == 0)
        return;

    uint64_t virtualAddress = AlignDown((uint64_t)address, PAGE_SIZE);
    uint64_t pageCount = AlignUp(size + ((uint64_t)address - virtualAddress), PAGE_SIZE) / PAGE_SIZE;

    for (uint64_t index = 0; index < pageCount; index++)
    {
        uint64_t physicalAddress = UnmapSinglePage(virtualAddress + index * PAGE_SIZE);
        if (physicalAddress != 0)
            FreePhysicalPage((void *)physicalAddress);
    }
}

uint64_t GetFreeMemorySize(void) { return FREE_MEMORY_SIZE; }

uint64_t GetUsedMemorySize(void) { return USED_MEMORY_SIZE; }

uint64_t GetPhysicalAddress(void *address)
{
    if (address == NULL)
        return 0;

    return GetMappedPhysicalAddress((uint64_t)address);
}

static uint64_t AlignUp(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint64_t AlignDown(uint64_t value, uint64_t alignment)
{
    return value & ~(alignment - 1);
}

static void AddFreePhysicalPages(uint64_t base_address, uint64_t page_count)
{
    if (page_count == 0 || FREE_MEMORY_REGION_COUNT >= MAX_FREE_MEMORY_REGIONS)
        return;

    uint64_t insertIndex = 0;
    while (insertIndex < FREE_MEMORY_REGION_COUNT &&
           FREE_MEMORY_REGIONS[insertIndex].baseAddress < base_address)
        insertIndex++;

    for (uint64_t index = FREE_MEMORY_REGION_COUNT; index > insertIndex; index--)
        FREE_MEMORY_REGIONS[index] = FREE_MEMORY_REGIONS[index - 1];

    FREE_MEMORY_REGIONS[insertIndex].baseAddress = base_address;
    FREE_MEMORY_REGIONS[insertIndex].pageCount = page_count;
    FREE_MEMORY_REGION_COUNT++;
    FREE_MEMORY_SIZE += page_count * PAGE_SIZE;
}

static void RemoveFreeMemoryRegion(uint64_t region_index)
{
    for (uint64_t index = region_index; index + 1 < FREE_MEMORY_REGION_COUNT; index++)
        FREE_MEMORY_REGIONS[index] = FREE_MEMORY_REGIONS[index + 1];

    if (FREE_MEMORY_REGION_COUNT > 0)
        FREE_MEMORY_REGION_COUNT--;
}

static void MergeFreeMemoryRegions(void)
{
    uint64_t index = 0;

    while (index + 1 < FREE_MEMORY_REGION_COUNT)
    {
        uint64_t currentEnd =
            FREE_MEMORY_REGIONS[index].baseAddress + FREE_MEMORY_REGIONS[index].pageCount * PAGE_SIZE;
        uint64_t nextBase = FREE_MEMORY_REGIONS[index + 1].baseAddress;

        if (currentEnd >= nextBase)
        {
            uint64_t nextEnd =
                FREE_MEMORY_REGIONS[index + 1].baseAddress +
                FREE_MEMORY_REGIONS[index + 1].pageCount * PAGE_SIZE;
            uint64_t mergedEnd = currentEnd > nextEnd ? currentEnd : nextEnd;

            FREE_MEMORY_REGIONS[index].pageCount =
                (mergedEnd - FREE_MEMORY_REGIONS[index].baseAddress) / PAGE_SIZE;
            RemoveFreeMemoryRegion(index + 1);
            continue;
        }

        index++;
    }
}

static void *AllocatePhysicalPage(void)
{
    for (uint64_t index = 0; index < FREE_MEMORY_REGION_COUNT; index++)
    {
        FREE_MEMORY_REGION *region = &FREE_MEMORY_REGIONS[index];
        if (region->pageCount == 0)
            continue;

        uint64_t address = region->baseAddress;
        region->baseAddress += PAGE_SIZE;
        region->pageCount--;

        if (region->pageCount == 0)
            RemoveFreeMemoryRegion(index);

        FREE_MEMORY_SIZE -= PAGE_SIZE;
        USED_MEMORY_SIZE += PAGE_SIZE;
        return (void *)address;
    }

    return NULL;
}

static void FreePhysicalPage(void *address)
{
    AddFreePhysicalPages((uint64_t)address, 1);
    MergeFreeMemoryRegions();

    if (USED_MEMORY_SIZE >= PAGE_SIZE)
        USED_MEMORY_SIZE -= PAGE_SIZE;
}

static uint64_t *GetOrCreatePageTable(uint64_t *table, uint64_t index, uint64_t flags)
{
    if ((table[index] & PAGE_PRESENT) != 0)
        return (uint64_t *)(table[index] & PAGE_MASK);

    uint64_t *nextTable = AllocatePhysicalPage();
    if (nextTable == NULL)
        return NULL;

    memset(nextTable, 0, PAGE_SIZE);
    table[index] = ((uint64_t)nextTable & PAGE_MASK) | PAGE_PRESENT | PAGE_READWRITE | flags;

    return nextTable;
}

static uint64_t *GetPageTable(uint64_t *table, uint64_t index)
{
    if ((table[index] & PAGE_PRESENT) == 0 || (table[index] & PAGE_LARGE) != 0)
        return NULL;

    return (uint64_t *)(table[index] & PAGE_MASK);
}

static void MapSinglePage(uint64_t virtual_address, uint64_t physical_address, uint64_t flags)
{
    uint64_t pml4Index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdptIndex = (virtual_address >> 30) & 0x1FF;
    uint64_t pdIndex = (virtual_address >> 21) & 0x1FF;
    uint64_t ptIndex = (virtual_address >> 12) & 0x1FF;

    uint64_t *pdpt = GetOrCreatePageTable(PAGE_MAP_LEVEL_4, pml4Index, 0);
    if (pdpt == NULL)
        return;

    uint64_t *pd = GetOrCreatePageTable(pdpt, pdptIndex, 0);
    if (pd == NULL)
        return;

    uint64_t *pt = GetOrCreatePageTable(pd, pdIndex, 0);
    if (pt == NULL)
        return;

    pt[ptIndex] = (physical_address & PAGE_MASK) | flags;
    InvalidateTLB((void *)virtual_address);
}

static uint64_t UnmapSinglePage(uint64_t virtual_address)
{
    uint64_t physicalAddress = GetMappedPhysicalAddress(virtual_address);
    if (physicalAddress == 0)
        return 0;

    uint64_t pml4Index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdptIndex = (virtual_address >> 30) & 0x1FF;
    uint64_t pdIndex = (virtual_address >> 21) & 0x1FF;
    uint64_t ptIndex = (virtual_address >> 12) & 0x1FF;

    uint64_t *pdpt = GetPageTable(PAGE_MAP_LEVEL_4, pml4Index);
    if (pdpt == NULL)
        return 0;

    uint64_t *pd = GetPageTable(pdpt, pdptIndex);
    if (pd == NULL)
        return 0;

    uint64_t *pt = GetPageTable(pd, pdIndex);
    if (pt == NULL)
        return 0;

    pt[ptIndex] = 0;
    InvalidateTLB((void *)virtual_address);

    return physicalAddress & PAGE_MASK;
}

static uint64_t GetMappedPhysicalAddress(uint64_t virtual_address)
{
    uint64_t pml4Index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdptIndex = (virtual_address >> 30) & 0x1FF;
    uint64_t pdIndex = (virtual_address >> 21) & 0x1FF;
    uint64_t ptIndex = (virtual_address >> 12) & 0x1FF;

    uint64_t *pdpt = GetPageTable(PAGE_MAP_LEVEL_4, pml4Index);
    if (pdpt == NULL)
        return 0;

    uint64_t *pd = GetPageTable(pdpt, pdptIndex);
    if (pd == NULL)
        return 0;

    uint64_t *pt = GetPageTable(pd, pdIndex);
    if (pt == NULL || (pt[ptIndex] & PAGE_PRESENT) == 0)
        return 0;

    return (pt[ptIndex] & PAGE_MASK) | (virtual_address & PAGE_OFFSET);
}
