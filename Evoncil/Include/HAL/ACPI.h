/** HAL/ACPI.h
 *
 * (C) Charity Enol
 *
 * 跨平台 ACPI 资产管理器头文件。
 */

#ifndef HAL_ACPI_H
#define HAL_ACPI_H

#include <stdint.h>
#include <stdbool.h>

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
    uint32_t VectorBase;
} __attribute__((packed)) MADT_IO_APIC;

typedef struct
{
    MADT_ENTRY_HEADER Header;
    uint8_t Bus;
    uint8_t Source;
    uint32_t Vector;
    uint16_t Flags;
} __attribute__((packed)) MADT_INTERRUPT_SOURCE_OVERRIDE;

typedef struct
{
    MADT_ENTRY_HEADER Header;
    uint16_t Reserved;
    uint64_t LocalApicAddress;
} __attribute__((packed)) MADT_LOCAL_APIC_ADDRESS_OVERRIDE;


void InitACPI(void *acpi_root);
void *AcpiFindTable(uint32_t signature);

#endif // HAL_ACPI_H