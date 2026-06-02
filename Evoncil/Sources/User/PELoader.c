/** User/PELoader.c
 *
 * (C) Charity Enol
 *
 * PE32+ 加载器和局部 Windows API 兼容层。
 */

#include <User/PELoader.h>
#include <User/POSIX.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define IMAGE_DOS_SIGNATURE 0x5A4Du
#define IMAGE_NT_SIGNATURE 0x00004550u
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20Bu

#define STD_INPUT_HANDLE (-10)
#define STD_OUTPUT_HANDLE (-11)
#define STD_ERROR_HANDLE (-12)

typedef struct
{
    uint16_t Magic;
    uint8_t Padding0[58];
    int32_t PEHeaderOffset;
} __attribute__((packed)) IMAGE_DOS_HEADER;

typedef struct
{
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} __attribute__((packed)) IMAGE_FILE_HEADER;

typedef struct
{
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
} __attribute__((packed)) IMAGE_OPTIONAL_HEADER64_MIN;

typedef struct
{
    uint8_t Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} __attribute__((packed)) IMAGE_SECTION_HEADER;

typedef struct
{
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
} __attribute__((packed)) IMAGE_NT_HEADERS_MIN;

/**
 * 检查指定文件范围是否完整存在于缓冲区中。
 */
static bool IsRangeInside(uint64_t offset, uint64_t size, uint64_t file_size);

/**
 * 根据 RVA 在 PE 节表中查找对应的文件指针。
 */
static const void *ConvertRvaToFilePointer(
    const void *file_base,
    uint64_t file_size,
    const IMAGE_SECTION_HEADER *sections,
    uint16_t section_count,
    uint32_t rva);

/**
 * Windows API: GetStdHandle。
 */
static void *WinGetStdHandle(int32_t handle);

/**
 * Windows API: WriteFile。
 */
static int WinWriteFile(
    void *handle,
    const void *buffer,
    uint32_t bytes_to_write,
    uint32_t *bytes_written,
    void *overlapped);

/**
 * Windows API: ReadFile。
 */
static int WinReadFile(
    void *handle,
    void *buffer,
    uint32_t bytes_to_read,
    uint32_t *bytes_read,
    void *overlapped);

/**
 * Windows API: ExitProcess。
 */
static void WinExitProcess(uint32_t exit_code);

bool LoadPEImage(const void *file_base, uint64_t file_size, PE_IMAGE *image)
{
    if (file_base == NULL || image == NULL)
        return false;

    if (!IsRangeInside(0, sizeof(IMAGE_DOS_HEADER), file_size))
        return false;

    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)file_base;
    if (dos->Magic != IMAGE_DOS_SIGNATURE || dos->PEHeaderOffset <= 0)
        return false;

    uint64_t ntOffset = (uint64_t)dos->PEHeaderOffset;
    if (!IsRangeInside(ntOffset, sizeof(IMAGE_NT_HEADERS_MIN), file_size))
        return false;

    const IMAGE_NT_HEADERS_MIN *nt =
        (const IMAGE_NT_HEADERS_MIN *)((const uint8_t *)file_base + ntOffset);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return false;

    uint64_t optionalOffset = ntOffset + sizeof(IMAGE_NT_HEADERS_MIN);
    if (!IsRangeInside(optionalOffset, sizeof(IMAGE_OPTIONAL_HEADER64_MIN), file_size))
        return false;

    const IMAGE_OPTIONAL_HEADER64_MIN *optional =
        (const IMAGE_OPTIONAL_HEADER64_MIN *)((const uint8_t *)file_base + optionalOffset);
    if (optional->Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;

    uint64_t sectionOffset = optionalOffset + nt->FileHeader.SizeOfOptionalHeader;
    uint64_t sectionSize = (uint64_t)nt->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
    if (!IsRangeInside(sectionOffset, sectionSize, file_size))
        return false;

    const IMAGE_SECTION_HEADER *sections =
        (const IMAGE_SECTION_HEADER *)((const uint8_t *)file_base + sectionOffset);

    const void *entryPoint = ConvertRvaToFilePointer(
        file_base,
        file_size,
        sections,
        nt->FileHeader.NumberOfSections,
        optional->AddressOfEntryPoint);
    if (entryPoint == NULL)
        return false;

    image->FileBase = file_base;
    image->FileSize = file_size;
    image->PreferredImageBase = optional->ImageBase;
    image->ImageSize = optional->SizeOfImage;
    image->EntryPointRVA = optional->AddressOfEntryPoint;
    image->EntryPoint = entryPoint;
    return true;
}

void *ResolveWindowsAPI(const char *name)
{
    if (name == NULL)
        return NULL;

    if (strcmp(name, "GetStdHandle") == 0)
        return WinGetStdHandle;

    if (strcmp(name, "WriteFile") == 0)
        return WinWriteFile;

    if (strcmp(name, "ReadFile") == 0)
        return WinReadFile;

    if (strcmp(name, "ExitProcess") == 0)
        return WinExitProcess;

    return NULL;
}

static bool IsRangeInside(uint64_t offset, uint64_t size, uint64_t file_size)
{
    if (offset > file_size)
        return false;

    return size <= file_size - offset;
}

static const void *ConvertRvaToFilePointer(
    const void *file_base,
    uint64_t file_size,
    const IMAGE_SECTION_HEADER *sections,
    uint16_t section_count,
    uint32_t rva)
{
    for (uint16_t index = 0; index < section_count; index++)
    {
        const IMAGE_SECTION_HEADER *section = &sections[index];
        uint32_t virtualSize = section->VirtualSize;
        if (virtualSize < section->SizeOfRawData)
            virtualSize = section->SizeOfRawData;

        if (rva < section->VirtualAddress ||
            rva >= section->VirtualAddress + virtualSize)
            continue;

        uint32_t offsetInSection = rva - section->VirtualAddress;
        uint64_t fileOffset = (uint64_t)section->PointerToRawData + offsetInSection;
        if (!IsRangeInside(fileOffset, 1, file_size))
            return NULL;

        return (const uint8_t *)file_base + fileOffset;
    }

    return NULL;
}

static void *WinGetStdHandle(int32_t handle)
{
    switch (handle)
    {
    case STD_INPUT_HANDLE:
        return (void *)(intptr_t)0;
    case STD_OUTPUT_HANDLE:
        return (void *)(intptr_t)1;
    case STD_ERROR_HANDLE:
        return (void *)(intptr_t)2;
    default:
        return NULL;
    }
}

static int WinWriteFile(
    void *handle,
    const void *buffer,
    uint32_t bytes_to_write,
    uint32_t *bytes_written,
    void *overlapped)
{
    (void)overlapped;

    int64_t result = write((int)(intptr_t)handle, buffer, bytes_to_write);
    if (bytes_written != NULL)
        *bytes_written = result < 0 ? 0 : (uint32_t)result;

    return result >= 0;
}

static int WinReadFile(
    void *handle,
    void *buffer,
    uint32_t bytes_to_read,
    uint32_t *bytes_read,
    void *overlapped)
{
    (void)overlapped;

    int64_t result = read((int)(intptr_t)handle, buffer, bytes_to_read);
    if (bytes_read != NULL)
        *bytes_read = result < 0 ? 0 : (uint32_t)result;

    return result >= 0;
}

static void WinExitProcess(uint32_t exit_code)
{
    exit((int)exit_code);
}
