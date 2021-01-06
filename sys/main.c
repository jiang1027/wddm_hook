#include "filter.h"
#include "trace.h"
#include "dlpapi.h"


static NTSTATUS commonDispatch(PDEVICE_OBJECT devObj, PIRP irp)
{
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(irp);

	UNREFERENCED_PARAMETER(devObj);

	pr_debug("-> commonDispatch(), Major(%s) Minor(%d)\n",
		MajorFunctionStr[irpStack->MajorFunction], irpStack->MinorFunction);

	switch (irpStack->MajorFunction)
	{
	case IRP_MJ_CREATE:
		break;
	case IRP_MJ_CLEANUP:
		break;
	case IRP_MJ_CLOSE:
		break;

	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
		pr_info("  IoControlCode(0x%08x)\n", 
			irpStack->Parameters.DeviceIoControl.IoControlCode);

		switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
		case 0x23003F: // win7 DxgkInitialize IoControlCode
			irp->IoStatus.Information = 0;
			irp->IoStatus.Status = STATUS_SUCCESS;

			if (irp->UserBuffer) {
				irp->IoStatus.Information = sizeof(PDXGKRNL_DPIINITIALIZE);
				*((PDXGKRNL_DPIINITIALIZE*)irp->UserBuffer) = DpiInitialize;
			}

			IoCompleteRequest(irp, IO_NO_INCREMENT);

			return STATUS_SUCCESS;

		case 0x230047: // win10 DxgkInitialize IoControlCode
			irp->IoStatus.Information = 0;
			irp->IoStatus.Status = STATUS_SUCCESS;

			if (irp->UserBuffer) {
				irp->IoStatus.Information = sizeof(PDXGKRNL_DPIINITIALIZE);
				*((PDXGKRNL_DPIINITIALIZE*)irp->UserBuffer) = Win10MonitorDpiInitialize;
			}

			IoCompleteRequest(irp, IO_NO_INCREMENT);

			return STATUS_SUCCESS;

		default:
			break;
		}
		break;
	}
	
	return call_lower_driver(irp);
}

NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT  DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status = STATUS_SUCCESS;

	pr_debug("-> loading wddmhook driver\n");

	UNREFERENCED_PARAMETER(RegistryPath);

	for (UCHAR i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i) {
		DriverObject->MajorFunction[i] = commonDispatch;
	}

	status = create_wddm_filter_ctrl_device(DriverObject);

	// no unload supported
	//
	DriverObject->DriverUnload = NULL;

	return status;
}

