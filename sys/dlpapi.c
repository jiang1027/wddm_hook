#include "dlpapi.h"

NTSTATUS DlpGetIoctlCode(ULONG* IoControlCode)
{
	RTL_OSVERSIONINFOW ver;
	NTSTATUS status;
	
	// by default, use old win7-style IOCTL code
	//
	*IoControlCode = 0x23003F; 

	status = RtlGetVersion(&ver);
	if (NT_SUCCESS(status) &&
		(ver.dwMajorVersion > 6 || // win10 or higher
			(ver.dwMajorVersion == 6 && ver.dwMinorVersion >= 2)) // win8 or server2012
		)
	{
		*IoControlCode = 0x230047;
	}
	return status;
}


NTSTATUS DlpLoadDxgkrnl(PFILE_OBJECT* FileObject, PDEVICE_OBJECT* DeviceObject)
{
	NTSTATUS status;
	LARGE_INTEGER interval;
	int retry = 10;
	WCHAR serviceNameStr[] = L"\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\SERVICES\\DXGKrnl";
	WCHAR objectNameStr[] = L"\\Device\\Dxgkrnl";
	UNICODE_STRING serviceName;
	UNICODE_STRING objectName;

	RtlInitUnicodeString(&serviceName, serviceNameStr);
	RtlInitUnicodeString(&objectName, objectNameStr);

	status = ZwLoadDriver(&serviceName);
	if (!NT_SUCCESS(status) && status != STATUS_IMAGE_ALREADY_LOADED) {
		return status;
	}

	for (; retry > 0; --retry) {
		status = IoGetDeviceObjectPointer(
			&objectName,
			GENERIC_READ | GENERIC_WRITE,
			FileObject,
			DeviceObject
		);
		if (NT_SUCCESS(status))
			break;

		interval.QuadPart = -1 * 1000 * 5;
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}

	return status;
}

NTSTATUS DlpCallSyncDeviceIoControl(
	PDEVICE_OBJECT DeviceObject,
	ULONG IoControlCode,
	BOOLEAN InternalDeviceIoControl,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength,
	ULONG* ReturnLength
)
{
	NTSTATUS status;
	PIRP Irp;
	IO_STATUS_BLOCK IoStatus = { 0 };
	KEVENT event;

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	Irp = IoBuildDeviceIoControlRequest(
		IoControlCode,
		DeviceObject,
		InputBuffer,
		InputBufferLength,
		OutputBuffer,
		OutputBufferLength,
		InternalDeviceIoControl,
		&event,
		&IoStatus
	);

	if (!Irp)
		return STATUS_INSUFFICIENT_RESOURCES;

	status = IoCallDriver(DeviceObject, Irp);
	if (status == STATUS_PENDING) {
		status = KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
		if (!NT_SUCCESS(status)) {
			return status;
		}
		status = IoStatus.Status;
	}

	if (NT_SUCCESS(status)) {
		*ReturnLength = (ULONG)IoStatus.Information;
	}
	return status;
}

