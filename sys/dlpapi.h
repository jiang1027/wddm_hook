#ifndef DLPAPI_INC
#define DLPAPI_INC

// reverse engineering achievements of DxgkInitialize() 
//

#include <ntddk.h>

NTSTATUS DlpGetIoctlCode(ULONG* IoControlCode);
NTSTATUS DlpLoadDxgkrnl(PFILE_OBJECT* FileObject, PDEVICE_OBJECT* DeviceObject);
NTSTATUS DlpCallSyncDeviceIoControl(
	PDEVICE_OBJECT DeviceObject,
	ULONG IoControlCode,
	BOOLEAN InternalDeviceIoControl,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength,
	ULONG* ReturnLength
);

#endif /* DLPAPI_INC */
/* end of file */
