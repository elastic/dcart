#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
#include <ntstrsafe.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;
DRIVER_INITIALIZE DriverEntry;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath);

NTSTATUS
DriverUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags);

NTSTATUS
InstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType);

VOID
InstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags);

VOID
InstanceTeardownComplete(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags);

NTSTATUS
InstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags);

VOID
OperationStatusCallback(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext);

FLT_POSTOP_CALLBACK_STATUS
PostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags);

BOOLEAN
DoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#pragma alloc_text(PAGE, InstanceQueryTeardown)
#pragma alloc_text(PAGE, InstanceSetup)
#pragma alloc_text(PAGE, InstanceTeardownStart)
#pragma alloc_text(PAGE, InstanceTeardownComplete)
#endif

FLT_PREOP_CALLBACK_STATUS
PreWrite(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION pNameInfo = NULL;

    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &pNameInfo);

    if (NT_SUCCESS(status))
    {
        status = FltParseFileNameInformation(pNameInfo);

        if (NT_SUCCESS(status))
        {
            ANSI_STRING aFileName;
            RtlUnicodeStringToAnsiString(&aFileName, &(pNameInfo->Name), TRUE);

            ULONG processId = FltGetRequestorProcessId(Data);

            UNICODE_STRING uDriverLogName;
            RtlInitUnicodeString(&uDriverLogName, L"\\Device\\HarddiskVolume2\\driver_log\\driver_log.dcart");

            UNICODE_STRING uPythonLogName;
            RtlInitUnicodeString(&uPythonLogName, L"\\Device\\HarddiskVolume2\\python_log\\python_log.dcart");

            LONG driverLogRetVal = RtlCompareUnicodeString(&(pNameInfo->Name), &uDriverLogName, TRUE);
            LONG pythonLogRetVal = RtlCompareUnicodeString(&(pNameInfo->Name), &uPythonLogName, TRUE);

            // this is to avoid filtering the writes created by this driver and the corresponding user space python process
            if ((4 != processId) && (0 != driverLogRetVal) && (0 != pythonLogRetVal))
            {
                DbgPrint("WRITE | PID: %u | FileName = %s\n", processId, aFileName.Buffer);

                UNICODE_STRING    uName;
                OBJECT_ATTRIBUTES objAttr;

                RtlInitUnicodeString(&uName, L"\\DosDevices\\C:\\driver_log\\driver_log.dcart");
                InitializeObjectAttributes(&objAttr, &uName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

                HANDLE   handle;
                NTSTATUS ntstatus;
                IO_STATUS_BLOCK ioStatusBlock;

                if (PASSIVE_LEVEL != KeGetCurrentIrql())
                {
                    return STATUS_INVALID_DEVICE_STATE;
                }

                ntstatus = ZwCreateFile(&handle,
                                        FILE_APPEND_DATA,
                                        &objAttr, &ioStatusBlock, NULL,
                                        FILE_ATTRIBUTE_NORMAL,
                                        FILE_SHARE_READ,
                                        FILE_OPEN_IF,
                                        FILE_SYNCHRONOUS_IO_NONALERT,
                                        NULL, 0);

                CHAR   write_buffer[512];
                size_t uLength;

                if (NT_SUCCESS(ntstatus))
                {
                    ntstatus = RtlStringCbPrintfA(write_buffer, sizeof(write_buffer), "WRITE|%u|%s\n", processId, aFileName.Buffer);
                    RtlStringCbLengthA(write_buffer, sizeof(write_buffer), &uLength);

                    LARGE_INTEGER ByteOffset;
                    ByteOffset.HighPart = -1;
                    ByteOffset.LowPart  = FILE_WRITE_TO_END_OF_FILE;
                    ntstatus            = ZwWriteFile(handle, NULL, NULL, NULL, &ioStatusBlock, write_buffer, (ULONG)uLength, &ByteOffset, NULL);
                    ZwClose(handle);
                }

                RtlFreeAnsiString(&aFileName);
            }
        }

        FltReleaseFileNameInformation(pNameInfo);
    }

    if (DoRequestOperationStatus(Data))
    {
        status = FltRequestOperationStatusCallback(Data, OperationStatusCallback, (PVOID)(++OperationStatusCtx));

        if (!NT_SUCCESS(status))
        {
            DbgPrint("DCART!PreWrite: FltRequestOperationStatusCallback Failed, status=%08x\n", status);
        }
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS
PreSetInfo(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION pNameInfo;
    PFILE_RENAME_INFORMATION pRenameInfo;
    ULONG processId = FltGetRequestorProcessId(Data);

    switch (Data->Iopb->Parameters.SetFileInformation.FileInformationClass)
    {
        case FileRenameInformation:
            FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED, &pNameInfo);
            FltReleaseFileNameInformation(pNameInfo);
            pRenameInfo = Data->Iopb->Parameters.SetFileInformation.InfoBuffer;

            ANSI_STRING aOldFileName;
            RtlUnicodeStringToAnsiString(&aOldFileName, &(pNameInfo->Name), TRUE);

            UNICODE_STRING uNewFileName;
            RtlInitUnicodeString(&uNewFileName, (PCWSTR)(&(pRenameInfo->FileName)));

            ANSI_STRING aNewFileName;
            RtlUnicodeStringToAnsiString(&aNewFileName, &uNewFileName, TRUE);

            UNICODE_STRING uDriverLogName;
            RtlInitUnicodeString(&uDriverLogName, L"\\Device\\HarddiskVolume2\\driver_log\\driver_log.dcart");

            LONG driverLogRetVal = RtlCompareUnicodeString(&(pNameInfo->Name), &uDriverLogName, TRUE);

            if ((4 != processId) && (0 != driverLogRetVal))
            {
                DbgPrint("RENAME | PID: %u | %s => %s\n", processId, aOldFileName.Buffer, aNewFileName.Buffer);

                UNICODE_STRING    uName;
                OBJECT_ATTRIBUTES objAttr;

                RtlInitUnicodeString(&uName, L"\\DosDevices\\C:\\driver_log\\driver_log.dcart");
                InitializeObjectAttributes(&objAttr, &uName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

                HANDLE   handle;
                NTSTATUS ntstatus;
                IO_STATUS_BLOCK ioStatusBlock;

                if (PASSIVE_LEVEL != KeGetCurrentIrql())
                {
                    return STATUS_INVALID_DEVICE_STATE;
                }

                ntstatus = ZwCreateFile(&handle,
                                        FILE_APPEND_DATA,
                                        &objAttr, &ioStatusBlock, NULL,
                                        FILE_ATTRIBUTE_NORMAL,
                                        FILE_SHARE_READ,
                                        FILE_OPEN_IF,
                                        FILE_SYNCHRONOUS_IO_NONALERT,
                                        NULL, 0);

                CHAR   write_buffer[512];
                size_t uLength;

                if (NT_SUCCESS(ntstatus)) 
                {
                    ntstatus = RtlStringCbPrintfA(write_buffer, sizeof(write_buffer), "RENAME|%u|%s|%s\n", processId, aOldFileName.Buffer, aNewFileName.Buffer);
                    RtlStringCbLengthA(write_buffer, sizeof(write_buffer), &uLength);

                    LARGE_INTEGER ByteOffset;
                    ByteOffset.HighPart = -1;
                    ByteOffset.LowPart  = FILE_WRITE_TO_END_OF_FILE;

                    ntstatus = ZwWriteFile(handle, NULL, NULL, NULL, &ioStatusBlock, write_buffer, (ULONG)uLength, &ByteOffset, NULL);
                    ZwClose(handle);
                }

                RtlFreeAnsiString(&aOldFileName);
                RtlFreeAnsiString(&aNewFileName);
            }

            break;

        default:
            break;
    }

    if (DoRequestOperationStatus(Data))
    {
        status = FltRequestOperationStatusCallback(Data, OperationStatusCallback, (PVOID)(++OperationStatusCtx));

        if (!NT_SUCCESS(status)) 
        {
            DbgPrint("DCART!PreSetInfo: FltRequestOperationStatusCallback Failed, status=%08x\n", status);
        }
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

// only registering two callbacks to handle write and setinfo (rename)

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    {IRP_MJ_WRITE,
     0,
     PreWrite,
     PostOperation},
    {IRP_MJ_SET_INFORMATION,
     0,
     PreSetInfo,
     PostOperation},
    {IRP_MJ_OPERATION_END}
};

CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION), //  Size
    FLT_REGISTRATION_VERSION, //  Version
    0,                        //  Flags
    NULL,                     //  Context
    Callbacks,                //  Operation callbacks
    DriverUnload,             //  MiniFilterUnload
    InstanceSetup,            //  InstanceSetup
    InstanceQueryTeardown,    //  InstanceQueryTeardown
    InstanceTeardownStart,    //  InstanceTeardownStart
    InstanceTeardownComplete, //  InstanceTeardownComplete
    NULL,                     //  GenerateFileName
    NULL,                     //  GenerateDestinationFileName
    NULL                      //  NormalizeNameComponent
};

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status;

    DbgPrint("DCART!DriverEntry\n");
    status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    if (NT_SUCCESS(status))
    {
        status = FltStartFiltering(gFilterHandle);
        
        if (NT_SUCCESS(status))
        {
            return status;
        }
    }

    FltUnregisterFilter(gFilterHandle);
    return status;
}

NTSTATUS
DriverUnload(
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();
    DbgPrint("DCART!DriverUnload\n");
    FltUnregisterFilter(gFilterHandle);
    return STATUS_SUCCESS;
}

NTSTATUS
InstanceSetup(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);
    PAGED_CODE();
    DbgPrint("DCART!InstanceSetup\n");
    return STATUS_SUCCESS;
}

NTSTATUS
InstanceQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();
    DbgPrint("DCART!InstanceQueryTeardown\n");
    return STATUS_SUCCESS;
}

VOID
InstanceTeardownStart(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();
    DbgPrint("DCART!InstanceTeardownStart\n");
}

VOID
InstanceTeardownComplete(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();
    DbgPrint("DCART!InstanceTeardownComplete\n");
}

VOID
OperationStatusCallback(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext)
{
    UNREFERENCED_PARAMETER(FltObjects);
    DbgPrint("DCART!OperationStatusCallback: Status=%08x ctx=%p IrpMj=%02x.%02x \"%s\"\n",
                OperationStatus,
                RequesterContext,
                ParameterSnapshot->MajorFunction,
                ParameterSnapshot->MinorFunction,
                FltGetIrpName(ParameterSnapshot->MajorFunction));
}

FLT_POSTOP_CALLBACK_STATUS
PostOperation(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

BOOLEAN
DoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data)
{
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
    //  return boolean state based on which operations we are interested in

    return (BOOLEAN)
             // check for oplock operations
             (((iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
               ((iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK)  ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK)   ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1) ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2)))

              ||

              // check for directy change notification
              ((iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL) && (iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY))
             );
}
