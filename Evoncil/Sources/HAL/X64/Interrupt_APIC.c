/** HAL/X64/Interrupt_APIC.c
 *
 * (C) Charity Enol
 *
 * Local APIC / IO APIC 初始化。
 */

#include <HAL/HAL.h>
#include <HAL/X64/CPU.h>
#include <HAL/X64/Registers.h>
#include <HAL/X64/Interrupt.h>
#include <Noyau/Memory.h>

#include <stdbool.h>
#include <stddef.h>

#define IA32_APIC_BASE_MSR 0x1B
#define IA32_APIC_BASE_X2APIC 0x400
#define IA32_APIC_BASE_ENABLE 0x800
#define IA32_X2APIC_MSR_BASE 0x800
#define CPUID_FEATURE_X2APIC (1u << 21)

#define LOCAL_APIC_ID 0x020
#define LOCAL_APIC_EOI 0x0B0
#define LOCAL_APIC_SPURIOUS 0x0F0
#define LOCAL_APIC_TPR 0x080
#define LOCAL_APIC_LVT_TIMER 0x320
#define LOCAL_APIC_TIMER_INITIAL_COUNT 0x380
#define LOCAL_APIC_TIMER_DIVIDE 0x3E0

#define LOCAL_APIC_TIMER_VECTOR 32
#define LOCAL_APIC_TIMER_PERIODIC (1u << 17)
#define LOCAL_APIC_SPURIOUS_VECTOR 255
#define LOCAL_APIC_MMIO_SIZE 0x1000
#define IO_APIC_MMIO_SIZE 0x1000

#define IO_APIC_REGISTER_SELECT 0x00
#define IO_APIC_REGISTER_WINDOW 0x10
#define IO_APIC_VERSION 0x01
#define IO_APIC_REDIRECTION_BASE 0x10

#define IO_APIC_REDIRECTION_MASKED (1u << 16)
#define IO_APIC_REDIRECTION_ACTIVE_LOW (1u << 13)
#define IO_APIC_REDIRECTION_LEVEL_TRIGGERED (1u << 15)

#define ACPI_SIGNATURE_MADT 0x43495041u

typedef struct
{
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t Reserved[3];
} __attribute__((packed)) RSDP_DESCRIPTOR;

typedef struct
{
    uint32_t Signature;
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed)) ACPI_TABLE_HEADER;

typedef struct
{
    ACPI_TABLE_HEADER Header;
    uint64_t Entries[];
} __attribute__((packed)) XSDT;

typedef struct
{
    ACPI_TABLE_HEADER Header;
    uint32_t Entries[];
} __attribute__((packed)) RSDT;

typedef struct
{
    ACPI_TABLE_HEADER Header;
    uint32_t LocalApicAddress;
    uint32_t Flags;
} __attribute__((packed)) MADT;

typedef struct
{
    uint8_t Type;
    uint8_t Length;
} __attribute__((packed)) MADT_ENTRY_HEADER;

typedef struct
{
    MADT_ENTRY_HEADER Header;
    uint8_t IOApicID;
    uint8_t Reserved;
    uint32_t IOApicAddress;
    uint32_t GlobalSystemInterruptBase;
} __attribute__((packed)) MADT_IO_APIC;

typedef struct
{
    MADT_ENTRY_HEADER Header;
    uint8_t Bus;
    uint8_t Source;
    uint32_t GlobalSystemInterrupt;
    uint16_t Flags;
} __attribute__((packed)) MADT_INTERRUPT_SOURCE_OVERRIDE;

typedef struct
{
    MADT_ENTRY_HEADER Header;
    uint16_t Reserved;
    uint64_t LocalApicAddress;
} __attribute__((packed)) MADT_LOCAL_APIC_ADDRESS_OVERRIDE;

typedef struct
{
    volatile uint32_t *Address;
    uint32_t GlobalSystemInterruptBase;
    uint32_t RedirectionCount;
} IO_APIC;

typedef struct
{
    uint8_t Source;
    uint32_t GlobalSystemInterrupt;
    uint16_t Flags;
} INTERRUPT_OVERRIDE;

typedef enum
{
    APIC_MODE_NONE = 0,
    APIC_MODE_XAPIC,
    APIC_MODE_X2APIC
} APIC_MODE;

static volatile uint32_t *LOCAL_APIC = NULL;
static APIC_MODE LOCAL_APIC_MODE = APIC_MODE_NONE;
static IO_APIC IO_APICS[8];
static uint32_t IO_APIC_COUNT = 0;
static INTERRUPT_OVERRIDE INTERRUPT_OVERRIDES[16];
static uint32_t INTERRUPT_OVERRIDE_COUNT = 0;
static uint64_t LOCAL_APIC_PHYSICAL_ADDRESS = 0xFEE00000;
static bool APIC_READY = false;

static MADT *FindMADT(void *acpi_root);
static ACPI_TABLE_HEADER *FindACPITable(void *acpi_root, uint32_t signature);
static bool IsRSDP20(const RSDP_DESCRIPTOR *rsdp);
static void ParseMADT(MADT *madt);
static void InitLocalAPIC(void);
static void InitLocalAPICTimer(void);
static void InitIOAPICs(void);
static bool IsX2APICSupported(void);
static uint32_t GetX2APICMSR(uint32_t register_offset);
static uint32_t ReadLocalAPIC(uint32_t register_offset);
static void WriteLocalAPIC(uint32_t register_offset, uint32_t value);
static uint32_t ReadIOAPIC(IO_APIC *io_apic, uint32_t register_index);
static void WriteIOAPIC(IO_APIC *io_apic, uint32_t register_index, uint32_t value);
static IO_APIC *FindIOAPIC(uint32_t global_system_interrupt);
static uint32_t GetHardwareInterruptGSI(uint8_t irq);
static uint16_t GetHardwareInterruptFlags(uint8_t irq);
static uint32_t GetCurrentLocalAPICID(void);
static bool SignatureEquals(const char *signature, const char *target);

void InitAPIC(void *acpi_root)
{
    APIC_READY = false;
    IO_APIC_COUNT = 0;
    INTERRUPT_OVERRIDE_COUNT = 0;
    LOCAL_APIC_PHYSICAL_ADDRESS = 0xFEE00000;
    LOCAL_APIC = NULL;
    LOCAL_APIC_MODE = APIC_MODE_NONE;

    MADT *madt = FindMADT(acpi_root);
    if (madt != NULL)
        ParseMADT(madt);

    InitLocalAPIC();
    InitIOAPICs();

    APIC_READY = LOCAL_APIC_MODE != APIC_MODE_NONE;
}

void SendEndOfInterrupt(void)
{
    if (LOCAL_APIC_MODE != APIC_MODE_NONE)
        WriteLocalAPIC(LOCAL_APIC_EOI, 0);
}

void SetHardwareInterrupt(uint8_t irq, uint8_t vector)
{
    if (!APIC_READY)
        return;

    uint32_t globalSystemInterrupt = GetHardwareInterruptGSI(irq);
    IO_APIC *ioApic = FindIOAPIC(globalSystemInterrupt);
    if (ioApic == NULL)
        return;

    uint32_t pin = globalSystemInterrupt - ioApic->GlobalSystemInterruptBase;
    uint16_t flags = GetHardwareInterruptFlags(irq);
    uint32_t low = vector;

    if ((flags & 0x3) == 0x3)
        low |= IO_APIC_REDIRECTION_ACTIVE_LOW;

    if (((flags >> 2) & 0x3) == 0x3)
        low |= IO_APIC_REDIRECTION_LEVEL_TRIGGERED;

    uint32_t high = (GetCurrentLocalAPICID() & 0xFF) << 24;

    WriteIOAPIC(ioApic, IO_APIC_REDIRECTION_BASE + pin * 2 + 1, high);
    WriteIOAPIC(ioApic, IO_APIC_REDIRECTION_BASE + pin * 2, low);
}

const char *GetInterruptControllerName(void)
{
    switch (LOCAL_APIC_MODE)
    {
    case APIC_MODE_X2APIC:
        return "x2APIC";
    case APIC_MODE_XAPIC:
        return "xAPIC";
    default:
        return "none";
    }
}

uint32_t GetLocalAPICID(void)
{
    return GetCurrentLocalAPICID();
}

uint32_t GetMSIMessageAddress(void)
{
    uint32_t apicID = GetCurrentLocalAPICID() & 0xFF;
    return 0xFEE00000u | (apicID << 12);
}

uint32_t GetMSIMessageData(uint8_t vector)
{
    return vector;
}

static MADT *FindMADT(void *acpi_root)
{
    return (MADT *)FindACPITable(acpi_root, ACPI_SIGNATURE_MADT);
}

static ACPI_TABLE_HEADER *FindACPITable(void *acpi_root, uint32_t signature)
{
    if (acpi_root == NULL)
        return NULL;

    RSDP_DESCRIPTOR *rsdp = (RSDP_DESCRIPTOR *)acpi_root;
    if (!SignatureEquals(rsdp->Signature, "RSD PTR "))
        return NULL;

    if (IsRSDP20(rsdp) && rsdp->XsdtAddress != 0)
    {
        XSDT *xsdt = (XSDT *)(uintptr_t)rsdp->XsdtAddress;
        uint32_t entryCount = (xsdt->Header.Length - sizeof(ACPI_TABLE_HEADER)) / sizeof(uint64_t);

        for (uint32_t index = 0; index < entryCount; index++)
        {
            ACPI_TABLE_HEADER *table = (ACPI_TABLE_HEADER *)(uintptr_t)xsdt->Entries[index];
            if (table->Signature == signature)
                return table;
        }
    }

    if (rsdp->RsdtAddress != 0)
    {
        RSDT *rsdt = (RSDT *)(uintptr_t)rsdp->RsdtAddress;
        uint32_t entryCount = (rsdt->Header.Length - sizeof(ACPI_TABLE_HEADER)) / sizeof(uint32_t);

        for (uint32_t index = 0; index < entryCount; index++)
        {
            ACPI_TABLE_HEADER *table = (ACPI_TABLE_HEADER *)(uintptr_t)rsdt->Entries[index];
            if (table->Signature == signature)
                return table;
        }
    }

    return NULL;
}

static bool IsRSDP20(const RSDP_DESCRIPTOR *rsdp)
{
    return rsdp->Revision >= 2 && rsdp->Length >= sizeof(RSDP_DESCRIPTOR);
}

static void ParseMADT(MADT *madt)
{
    LOCAL_APIC_PHYSICAL_ADDRESS = madt->LocalApicAddress;

    uint8_t *entry = (uint8_t *)madt + sizeof(MADT);
    uint8_t *end = (uint8_t *)madt + madt->Header.Length;

    while (entry + sizeof(MADT_ENTRY_HEADER) <= end)
    {
        MADT_ENTRY_HEADER *header = (MADT_ENTRY_HEADER *)entry;
        if (header->Length == 0 || entry + header->Length > end)
            break;

        switch (header->Type)
        {
        case 1:
        {
            if (IO_APIC_COUNT >= sizeof(IO_APICS) / sizeof(IO_APICS[0]))
                break;

            MADT_IO_APIC *ioApic = (MADT_IO_APIC *)entry;
            volatile uint32_t *mappedAddress =
                (volatile uint32_t *)MapDeviceMemory(ioApic->IOApicAddress, IO_APIC_MMIO_SIZE);
            if (mappedAddress == NULL)
                break;

            IO_APICS[IO_APIC_COUNT].Address = mappedAddress;
            IO_APICS[IO_APIC_COUNT].GlobalSystemInterruptBase = ioApic->GlobalSystemInterruptBase;
            IO_APICS[IO_APIC_COUNT].RedirectionCount = 0;
            IO_APIC_COUNT++;
            break;
        }
        case 2:
        {
            if (INTERRUPT_OVERRIDE_COUNT >= sizeof(INTERRUPT_OVERRIDES) / sizeof(INTERRUPT_OVERRIDES[0]))
                break;

            MADT_INTERRUPT_SOURCE_OVERRIDE *override = (MADT_INTERRUPT_SOURCE_OVERRIDE *)entry;
            INTERRUPT_OVERRIDES[INTERRUPT_OVERRIDE_COUNT].Source = override->Source;
            INTERRUPT_OVERRIDES[INTERRUPT_OVERRIDE_COUNT].GlobalSystemInterrupt =
                override->GlobalSystemInterrupt;
            INTERRUPT_OVERRIDES[INTERRUPT_OVERRIDE_COUNT].Flags = override->Flags;
            INTERRUPT_OVERRIDE_COUNT++;
            break;
        }
        case 5:
        {
            MADT_LOCAL_APIC_ADDRESS_OVERRIDE *override =
                (MADT_LOCAL_APIC_ADDRESS_OVERRIDE *)entry;
            LOCAL_APIC_PHYSICAL_ADDRESS = override->LocalApicAddress;
            break;
        }
        default:
            break;
        }

        entry += header->Length;
    }
}

static void InitLocalAPIC(void)
{
    uint64_t apicBase = ReadMSR(IA32_APIC_BASE_MSR);

    if (IsX2APICSupported())
    {
        apicBase |= IA32_APIC_BASE_ENABLE | IA32_APIC_BASE_X2APIC;
        WriteMSR(IA32_APIC_BASE_MSR, apicBase);
        LOCAL_APIC_MODE = APIC_MODE_X2APIC;
    }
    else
    {
        apicBase &= (uint64_t)~IA32_APIC_BASE_X2APIC;
        apicBase &= 0xFFF;
        apicBase |= (LOCAL_APIC_PHYSICAL_ADDRESS & 0xFFFFFFFFFFFFF000ull);
        apicBase |= IA32_APIC_BASE_ENABLE;
        WriteMSR(IA32_APIC_BASE_MSR, apicBase);

        LOCAL_APIC = (volatile uint32_t *)MapDeviceMemory(
            LOCAL_APIC_PHYSICAL_ADDRESS,
            LOCAL_APIC_MMIO_SIZE);
        if (LOCAL_APIC == NULL)
            return;

        LOCAL_APIC_MODE = APIC_MODE_XAPIC;
    }

    WriteLocalAPIC(LOCAL_APIC_TPR, 0);
    WriteLocalAPIC(
        LOCAL_APIC_SPURIOUS,
        ReadLocalAPIC(LOCAL_APIC_SPURIOUS) | 0x100 | LOCAL_APIC_SPURIOUS_VECTOR);

    InitLocalAPICTimer();
}

static void InitLocalAPICTimer(void)
{
    WriteLocalAPIC(LOCAL_APIC_TIMER_DIVIDE, 0x3);
    WriteLocalAPIC(
        LOCAL_APIC_LVT_TIMER,
        LOCAL_APIC_TIMER_PERIODIC | LOCAL_APIC_TIMER_VECTOR);
    WriteLocalAPIC(LOCAL_APIC_TIMER_INITIAL_COUNT, 10000000u);
}

static void InitIOAPICs(void)
{
    for (uint32_t index = 0; index < IO_APIC_COUNT; index++)
    {
        IO_APIC *ioApic = &IO_APICS[index];
        uint32_t version = ReadIOAPIC(ioApic, IO_APIC_VERSION);
        ioApic->RedirectionCount = ((version >> 16) & 0xFF) + 1;

        for (uint32_t pin = 0; pin < ioApic->RedirectionCount; pin++)
        {
            WriteIOAPIC(ioApic, IO_APIC_REDIRECTION_BASE + pin * 2 + 1, 0);
            WriteIOAPIC(
                ioApic,
                IO_APIC_REDIRECTION_BASE + pin * 2,
                IO_APIC_REDIRECTION_MASKED);
        }
    }
}

static bool IsX2APICSupported(void)
{
    uint32_t ecx;

    ReadCPUID(1, 0, NULL, NULL, &ecx, NULL);
    return (ecx & CPUID_FEATURE_X2APIC) != 0;
}

static uint32_t GetX2APICMSR(uint32_t register_offset)
{
    return IA32_X2APIC_MSR_BASE + register_offset / 0x10;
}

static uint32_t ReadLocalAPIC(uint32_t register_offset)
{
    if (LOCAL_APIC_MODE == APIC_MODE_X2APIC)
        return (uint32_t)ReadMSR(GetX2APICMSR(register_offset));

    if (LOCAL_APIC == NULL)
        return 0;

    return LOCAL_APIC[register_offset / sizeof(uint32_t)];
}

static void WriteLocalAPIC(uint32_t register_offset, uint32_t value)
{
    if (LOCAL_APIC_MODE == APIC_MODE_X2APIC)
    {
        WriteMSR(GetX2APICMSR(register_offset), value);
        return;
    }

    if (LOCAL_APIC == NULL)
        return;

    LOCAL_APIC[register_offset / sizeof(uint32_t)] = value;
}

static uint32_t ReadIOAPIC(IO_APIC *io_apic, uint32_t register_index)
{
    io_apic->Address[IO_APIC_REGISTER_SELECT / sizeof(uint32_t)] = register_index;
    return io_apic->Address[IO_APIC_REGISTER_WINDOW / sizeof(uint32_t)];
}

static void WriteIOAPIC(IO_APIC *io_apic, uint32_t register_index, uint32_t value)
{
    io_apic->Address[IO_APIC_REGISTER_SELECT / sizeof(uint32_t)] = register_index;
    io_apic->Address[IO_APIC_REGISTER_WINDOW / sizeof(uint32_t)] = value;
}

static IO_APIC *FindIOAPIC(uint32_t global_system_interrupt)
{
    for (uint32_t index = 0; index < IO_APIC_COUNT; index++)
    {
        IO_APIC *ioApic = &IO_APICS[index];
        uint32_t end = ioApic->GlobalSystemInterruptBase + ioApic->RedirectionCount;

        if (global_system_interrupt >= ioApic->GlobalSystemInterruptBase &&
            global_system_interrupt < end)
            return ioApic;
    }

    return NULL;
}

static uint32_t GetHardwareInterruptGSI(uint8_t irq)
{
    for (uint32_t index = 0; index < INTERRUPT_OVERRIDE_COUNT; index++)
        if (INTERRUPT_OVERRIDES[index].Source == irq)
            return INTERRUPT_OVERRIDES[index].GlobalSystemInterrupt;

    return irq;
}

static uint16_t GetHardwareInterruptFlags(uint8_t irq)
{
    for (uint32_t index = 0; index < INTERRUPT_OVERRIDE_COUNT; index++)
        if (INTERRUPT_OVERRIDES[index].Source == irq)
            return INTERRUPT_OVERRIDES[index].Flags;

    return 0;
}

static uint32_t GetCurrentLocalAPICID(void)
{
    if (LOCAL_APIC_MODE == APIC_MODE_X2APIC)
        return ReadLocalAPIC(LOCAL_APIC_ID);

    if (LOCAL_APIC_MODE == APIC_MODE_XAPIC)
        return ReadLocalAPIC(LOCAL_APIC_ID) >> 24;

    return 0;
}

static bool SignatureEquals(const char *signature, const char *target)
{
    for (uint32_t index = 0; index < 8; index++)
        if (signature[index] != target[index])
            return false;

    return true;
}
