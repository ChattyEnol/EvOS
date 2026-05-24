/** HAL/ACPI.c
 *
 * (C) Charity Enol
 *
 * 跨平台 ACPI 资产管理器实现。
 */

#include <HAL/ACPI.h>
#include <stddef.h>

static void *AcpiRoot = NULL;
static bool IsRSDP20(const RSDP_DESCRIPTOR *rsdp);
static bool SignatureEquals(const char *signature, const char *target);

void InitACPI(void *acpi_root) { AcpiRoot = acpi_root; }

void *AcpiFindTable(uint32_t signature)
{
    if (AcpiRoot == NULL)
        return NULL;

    RSDP_DESCRIPTOR *rsdp = (RSDP_DESCRIPTOR *)AcpiRoot;
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

static bool SignatureEquals(const char *signature, const char *target)
{
    for (uint32_t index = 0; index < 8; index++)
        if (signature[index] != target[index])
            return false;

    return true;
}