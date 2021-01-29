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
				*((PDXGKRNL_DPIINITIALIZE*)irp->UserBuffer) = Win10DpiInitialize;
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

void WddmHookUnload(
	PDRIVER_OBJECT DriverObject
)
{
	pr_debug("-> WddmHookUnload\n");
}

NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT  DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	NTSTATUS status = STATUS_SUCCESS;

	pr_debug("-> loading wddmhook driver\n");

	UNREFERENCED_PARAMETER(RegistryPath);

	RtlZeroMemory(&Global, sizeof(Global));

	Global.DriverObject = DriverObject;
	KeInitializeSpinLock(&Global.spin_lock);
	InitializeListHead(&Global.vidpn_if_head);
	InitializeListHead(&Global.topology_if_head);

	Global.fEnumVirtualChild = FALSE;

	Global.fDumpSourceModeSet = TRUE;
	Global.fDumpPinnedSourceMode = TRUE;
	Global.nMaxSourceModeSetDump = -1;

	Global.fDumpTargetModeSet = TRUE;
	Global.fDumpPinnedTargetMode = TRUE;
	Global.fDumpPreferredTargetMode = TRUE;
	Global.nMaxTargetModeSetDump = -1;

	Global.nMaxVidPnDump = -1;
	Global.nVidPnDumped = 0;

	Global.fCreateHookDevice = FALSE;
	Global.fHookEnabled = FALSE;
	Global.fModifyConstrainingVidPn = FALSE;

	KeInitializeSpinLock(&Global.Lock);
	InitializeListHead(&Global.HookDriverList);

	if (Global.fCreateHookDevice) {
		for (UCHAR i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i) {
			DriverObject->MajorFunction[i] = commonDispatch;
		}

		status = create_wddm_filter_ctrl_device(DriverObject);
		DriverObject->DriverUnload = NULL;
	}
	else {
		// make driver unload-able
		//
		DriverObject->DriverUnload = WddmHookUnload;
		status = STATUS_UNSUCCESSFUL;
	}

	return status;
}

