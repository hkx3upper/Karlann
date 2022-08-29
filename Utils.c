
#include "utils.h"


PVOID
PocFindPattern(
    IN PCHAR Buffer,
    IN ULONG BufferSize,
    IN PCHAR Pattern,
    IN ULONG PatternSize
)
{
    ASSERT(NULL != Buffer);
    ASSERT(NULL != Pattern);


    PCHAR	p = Buffer;

    if (PatternSize == 0)
        return NULL;
    if (BufferSize < PatternSize)
        return NULL;
    BufferSize -= PatternSize;

    do {
        p = memchr(p, Pattern[0], BufferSize - (p - Buffer));
        if (p == NULL)
            break;

        if (memcmp(p, Pattern, PatternSize) == 0)
            return p;

        p++;
    } while (BufferSize - (p - Buffer) > 0); //-V555

    return NULL;
}


PVOID PocLookupImageSectionByName(
    _In_ PCHAR SectionName,
    _In_ PVOID DllBase,
    _Out_ PULONG SectionSize
)
{
    ASSERT(NULL != SectionName);
    ASSERT(NULL != DllBase);
    ASSERT(NULL != SectionSize);

    BOOLEAN bFound = FALSE;
    ULONG i;
    PVOID Section;
    IMAGE_NT_HEADERS64* NtHeaders = RtlImageNtHeader(DllBase);
    IMAGE_SECTION_HEADER* SectionTableEntry;

    if (SectionSize)
        *SectionSize = 0;

    SectionTableEntry = (PIMAGE_SECTION_HEADER)((PCHAR)NtHeaders +
        sizeof(ULONG) +
        sizeof(IMAGE_FILE_HEADER) +
        NtHeaders->FileHeader.SizeOfOptionalHeader);

    //
    // Locate section.
    //
    i = NtHeaders->FileHeader.NumberOfSections;

    while (i > 0)
    {
        if (!_strnicmp((PCHAR)SectionTableEntry->Name, SectionName, strlen(SectionName)))
        {
            bFound = TRUE;
            break;
        }

        i -= 1;
        SectionTableEntry += 1;
    }

    //
    // Section not found, abort scan.
    //
    if (!bFound)
        return NULL;

    Section = (PVOID)((ULONG_PTR)DllBase + SectionTableEntry->VirtualAddress);
    if (SectionSize)
        *SectionSize = SectionTableEntry->Misc.VirtualSize;

    return Section;
}
