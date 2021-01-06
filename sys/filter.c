#include "filter.h"
#include "ioctl.h"
#include "trace.h"
#include "dlpapi.h"

///global var
struct wddm_filter_t WddmHookGlobal = { 0 };

#define DEV_NAME L"\\Device\\WddmFilterCtrlDevice"
#define DOS_NAME L"\\DosDevices\\WddmFilterCtrlDevice"


static NTSTATUS create_ctrl_device()
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT devObj;
	UNICODE_STRING dev_name;
	UNICODE_STRING dos_name;

	RtlInitUnicodeString(&dev_name, DEV_NAME);
	RtlInitUnicodeString(&dos_name, DOS_NAME);

	status = IoCreateDevice(
		wf->driver_object,
		0,
		&dev_name,
		FILE_DEVICE_VIDEO,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&devObj);
	if (!NT_SUCCESS(status)) {
		pr_err("IoCreateDevice err=0x%X\n", status );
		return status;
	}

	status = IoCreateSymbolicLink(&dos_name, &dev_name);
	if (!NT_SUCCESS(status)) {
		pr_err("IoCreateSymbolicLink err=0x%X\n", status );
		IoDeleteDevice(devObj);
		return status;
	}

	// attach
	wf->dxgkrnl_nextDevice = IoAttachDeviceToDeviceStack(devObj, wf->dxgkrnl_pdoDevice);
	if (!wf->dxgkrnl_nextDevice) {
		pr_err("IoAttachDeviceToDeviceStack error.\n");
		IoDeleteDevice(devObj);
		IoDeleteSymbolicLink(&dos_name);
		return STATUS_NOT_FOUND;
	}
	wf->ctrl_devobj = devObj;

	// use the same DeviceType/Characteristics/Flags as 
	// the underlying device
	//
	devObj->DeviceType = wf->dxgkrnl_nextDevice->DeviceType;
	devObj->Characteristics = wf->dxgkrnl_nextDevice->Characteristics;
 	devObj->Flags |= wf->dxgkrnl_nextDevice->Flags & (DO_POWER_PAGABLE | DO_BUFFERED_IO | DO_DIRECT_IO);
	
	devObj->Flags &= ~DO_DEVICE_INITIALIZING;

	return status;
}


NTSTATUS create_wddm_filter_ctrl_device(PDRIVER_OBJECT drvObj)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG IoControlCode;
	ULONG returnLength;

	RtlZeroMemory(wf, sizeof(struct wddm_filter_t));

	wf->driver_object = drvObj;
	KeInitializeSpinLock(&wf->spin_lock);
	InitializeListHead(&wf->vidpn_if_head);
	InitializeListHead(&wf->topology_if_head);

	status = DlpLoadDxgkrnl(&wf->dxgkrnl_fileobj, &wf->dxgkrnl_pdoDevice);
	if (!NT_SUCCESS(status)) {
		pr_err("load dxgkrnl failed, status(0x%08x)\n", status);
		return status;
	}

	status = DlpGetIoctlCode(&IoControlCode);
	if (!NT_SUCCESS(status)) {
		pr_err("get IoControlCode failed, status(0x%08x)\n", status);
		return status;
	}

	for (; ; IoControlCode = 0x23003F) {
		status = DlpCallSyncDeviceIoControl(
			wf->dxgkrnl_pdoDevice,
			IoControlCode,
			TRUE,
			NULL,
			0,
			&wf->dxgkrnl_dpiInit,
			sizeof(wf->dxgkrnl_dpiInit),
			&returnLength
		);
		if (status != STATUS_INVALID_DEVICE_REQUEST)
			break;
		if (IoControlCode != 0x230047)
			break;
	}

	if (!NT_SUCCESS(status)) {
		pr_err("get DxgkDpiInit failed, status(0x%08x)\n", status);
		return status;
	}

	ASSERT(wf->dxgkrnl_dpiInit != NULL);

	pr_info("DxgkDpiInit = %p\n", wf->dxgkrnl_dpiInit);

	// create filter device
	//
	status = create_ctrl_device();
	if (!NT_SUCCESS(status)) {
		return status;
	}

	return status;
}


NTSTATUS call_lower_driver(PIRP irp)
{
	IoSkipCurrentIrpStackLocation(irp);
	return IoCallDriver(wf->dxgkrnl_nextDevice, irp);
}


NTSTATUS
Filter_DxgkDdiAddDevice(
	IN_CONST_PDEVICE_OBJECT     PhysicalDeviceObject,
	OUT_PPVOID                  MiniportDeviceContext
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiAddDevice);

	pr_debug("-> Filter_DxgkDdiAddDevice()\n");

	status = DriverInitData->DxgkDdiAddDevice(PhysicalDeviceObject, MiniportDeviceContext);

	pr_debug("<- Filter_DxgkDdiAddDevice(), status(0x%08x), MiniportDeviceContext(%p)\n",
		status, *MiniportDeviceContext);

	return status;
}


NTSTATUS
Filter_DxgkDdiStartDevice(
	IN_CONST_PVOID          MiniportDeviceContext,
	IN_PDXGK_START_INFO     DxgkStartInfo,
	IN_PDXGKRNL_INTERFACE   DxgkInterface,
	OUT_PULONG              NumberOfVideoPresentSources,
	OUT_PULONG              NumberOfChildren
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiStartDevice);

	pr_debug("-> Filter_DxgkDdiStartDevice()\n");

	status = DriverInitData->DxgkDdiStartDevice(
		MiniportDeviceContext,
		DxgkStartInfo,
		DxgkInterface,
		NumberOfVideoPresentSources,
		NumberOfChildren
	);

	pr_debug("<- Filter_DxgkDdiStartDevice(), status(0x%08x)\n", status);

	return status;
}


NTSTATUS
Filter_DxgkDdiStopDevice(
	IN_CONST_PVOID  MiniportDeviceContext
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiStopDevice);

	pr_debug("-> Filter_DxgkDdiStopDevice()\n");

	status = DriverInitData->DxgkDdiStopDevice(MiniportDeviceContext);

	pr_debug("<- Filter_DxgkDdiStopDevice(), status(0x%08x)\n", status);

	return status;
}


NTSTATUS
Filter_DxgkDdiRemoveDevice(
	IN_CONST_PVOID  MiniportDeviceContext
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiRemoveDevice);

	pr_debug("-> Filter_DxgkDdiRemoveDevice()\n");

	status = DriverInitData->DxgkDdiRemoveDevice(MiniportDeviceContext);

	pr_debug("<- Filter_DxgkDdiRemoveDevice(), status(0x%08x)\n", status);

	return status;
}


NTSTATUS
Filter_DxgkDdiQueryChildRelations(
	IN_CONST_PVOID                                                    MiniportDeviceContext,
	_Inout_updates_bytes_(ChildRelationsSize) PDXGK_CHILD_DESCRIPTOR  ChildRelations,
	_In_ ULONG                                                        ChildRelationsSize
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiQueryChildRelations);

	pr_debug("-> Filter_DxgkDdiQueryChildRelations()\n");

	status = DriverInitData->DxgkDdiQueryChildRelations(
		MiniportDeviceContext,
		ChildRelations,
		ChildRelationsSize
	);

	pr_debug("<- Filter_DxgkDdiQueryChildRelations(), status(0x%08x)\n", status);

	return status;
}

NTSTATUS
Filter_DxgkDdiQueryChildStatus(
	IN_CONST_PVOID              MiniportDeviceContext,
	INOUT_PDXGK_CHILD_STATUS    ChildStatus,
	IN_BOOLEAN                  NonDestructiveOnly
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiQueryChildStatus);

	pr_debug("-> Filter_DxgkDdiQueryChildStatus()\n");

	status = DriverInitData->DxgkDdiQueryChildStatus(
		MiniportDeviceContext,
		ChildStatus,
		NonDestructiveOnly
	);
	
	pr_debug("<- Filter_DxgkDdiQueryChildStatus(), status(0x%08x)\n", status);

	return status;
}

NTSTATUS
Filter_DxgkDdiQueryDeviceDescriptor(
	IN_CONST_PVOID                  MiniportDeviceContext,
	IN_ULONG                        ChildUid,
	INOUT_PDXGK_DEVICE_DESCRIPTOR   DeviceDescriptor
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiQueryDeviceDescriptor);

	pr_debug("-> Filter_DxgkDdiQueryDeviceDescriptor()\n");

	status = DriverInitData->DxgkDdiQueryDeviceDescriptor(
		MiniportDeviceContext,
		ChildUid,
		DeviceDescriptor
	);

	pr_debug("<- Filter_DxgkDdiQueryDeviceDescriptor(), status(0x%08x)\n", status);

	return status;
}

NTSTATUS
APIENTRY
Filter_DxgkDdiIsSupportedVidPn(
	IN_CONST_HANDLE                     hAdapter,
	INOUT_PDXGKARG_ISSUPPORTEDVIDPN     pIsSupportedVidPn
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiIsSupportedVidPn);

	pr_debug("-> Filter_DxgkDdiIsSupportedVidPn()\n");

	status = DriverInitData->DxgkDdiIsSupportedVidPn(
		hAdapter,
		pIsSupportedVidPn
	);

	pr_debug("<- Filter_DxgkDdiIsSupportedVidPn(), status(0x%08x)\n", status);

	return status;

}

NTSTATUS
APIENTRY
Filter_DxgkDdiEnumVidPnCofuncModality(
	IN_CONST_HANDLE                                     hAdapter,
	IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST     pEnumCofuncModality
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiEnumVidPnCofuncModality);

	pr_debug("-> Filter_DxgkDdiEnumVidPnCofuncModality()\n");

	status = DriverInitData->DxgkDdiEnumVidPnCofuncModality(
		hAdapter,
		pEnumCofuncModality
	);

	pr_debug("<- Filter_DxgkDdiEnumVidPnCofuncModality(), status(0x%08x)\n", status);
	return status;
}

NTSTATUS
APIENTRY
Filter_DxgkDdiSetVidPnSourceAddress(
	IN_CONST_HANDLE                             hAdapter,
	IN_CONST_PDXGKARG_SETVIDPNSOURCEADDRESS     pSetVidPnSourceAddress
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiSetVidPnSourceAddress);

	pr_debug("-> Filter_DxgkDdiSetVidPnSourceAddress()\n");

	status = DriverInitData->DxgkDdiSetVidPnSourceAddress(
		hAdapter,
		pSetVidPnSourceAddress
	);

	pr_debug("<- Filter_DxgkDdiSetVidPnSourceAddress(), status(0x%08x)\n", status);
	return status;
}


NTSTATUS
APIENTRY
Filter_DxgkDdiSetVidPnSourceVisibility(
	IN_CONST_HANDLE                             hAdapter,
	IN_CONST_PDXGKARG_SETVIDPNSOURCEVISIBILITY  pSetVidPnSourceVisibility
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiSetVidPnSourceVisibility);

	pr_debug("-> Filter_DxgkDdiSetVidPnSourceVisibility()\n");

	status = DriverInitData->DxgkDdiSetVidPnSourceVisibility(
		hAdapter,
		pSetVidPnSourceVisibility
	);

	pr_debug("<- Filter_DxgkDdiSetVidPnSourceVisibility(), status(0x%08x)\n", status);

	return status;
}

NTSTATUS
APIENTRY
Filter_DxgkDdiCommitVidPn(
	IN_CONST_HANDLE                         hAdapter,
	IN_CONST_PDXGKARG_COMMITVIDPN_CONST     pCommitVidPn
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData = &wf->orgDpiFunc;
	NTSTATUS status;

	ASSERT(DriverInitData->DxgkDdiCommitVidPn);

	pr_debug("-> Filter_DxgkDdiCommitVidPn()\n");

	status = DriverInitData->DxgkDdiCommitVidPn(
		hAdapter,
		pCommitVidPn
	);

	pr_debug("<- Filter_DxgkDdiCommitVidPn(), status(0x%08x)\n", status);

	return status;
}
