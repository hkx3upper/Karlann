#pragma once

#include <ntddk.h>
#include <ntddkbd.h>

const static ULONG gTraceFlags = 0x00000001;
extern POBJECT_TYPE* IoDriverObjectType;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define FlagOn(Flags,SingleFlag) ((BOOLEAN)(       \
    (((Flags) & (SingleFlag)) != 0 ? TRUE : FALSE) \
    )                                              \
)

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

#define POC_SPIN_LOCK_TAG               'Sltg'
#define POC_IRP_READ_QUEUE_TAG          'Rqtg'
#define POC_IRP_SYSTEMBUFFER_TAG        'Sbtg'

#define MAXIMUM_ITEMS_READ              10

/*
* kbdclass的DeviceExtension结构体某些项的偏移，
* 虽然未导出，但这两个值的偏移从Windows 8 x64开始，都是不变的
*/
#define SPIN_LOCK_OFFSET_DE             0xA0
#define READ_QUEUE_OFFSET_DE            0xA8

typedef struct _POC_READ_QUEUE
{
    LIST_ENTRY ListEntry;
    PIRP Irp;
}
POC_READ_QUEUE, * PPOC_READ_QUEUE;

typedef struct _POC_READ_QUEUE_ITEM
{
    LIST_ENTRY ReadQueueHead;
    PKSPIN_LOCK ReadQueueSpinLock;
}
POC_READ_QUEUE_ITEM, * PPOC_READ_QUEUE_ITEM;

typedef struct _POC_READ_FAKE_IRP_ITEM
{
    PIRP FakeIrp;
    PCHAR TempBuffer;
    ULONG TempBufferSize;
    ULONG KeyCount;
}
POC_READ_FAKE_IRP_ITEM, * PPOC_READ_FAKE_IRP_ITEM;

typedef NTSTATUS(*KeyboardClassReadCopyData)
(
    IN PVOID DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
PocDriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

VOID 
PocUnload(
    _In_ PDRIVER_OBJECT  DriverObject
);

VOID
PocReadQueueCleanup(
);

NTSTATUS
ObReferenceObjectByName(
    __in PUNICODE_STRING ObjectName,
    __in ULONG Attributes,
    __in_opt PACCESS_STATE AccessState,
    __in_opt ACCESS_MASK DesiredAccess,
    __in POBJECT_TYPE ObjectType,
    __in KPROCESSOR_MODE AccessMode,
    __inout_opt PVOID ParseContext,
    __out PVOID* Object
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, PocDriverEntry)
#endif
