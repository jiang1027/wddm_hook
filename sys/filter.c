#include "filter.h"
#include "ioctl.h"
#include "trace.h"
#include "dlpapi.h"
#include "helperapi.h"
#include "DxgkCb.h"

#define WDDM_HOOK_VIRTUAL_HDMI_CHILD_UID 0x11220

const BYTE SampleEDID[] = {
	0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,
	0x10,0xAC,0xE6,0xD0,0x55,0x5A,0x4A,0x30,
	0x24,0x1D,0x01,0x04,0xA5,0x3C,0x22,0x78,
	0xFB,0x6C,0xE5,0xA5,0x55,0x50,0xA0,0x23,
	0x0B,0x50,0x54,0x00,0x02,0x00,0xD1,0xC0,
	0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
	0x01,0x01,0x01,0x01,0x01,0x01,0x58,0xE3,
	0x00,0xA0,0xA0,0xA0,0x29,0x50,0x30,0x20,
	0x35,0x00,0x55,0x50,0x21,0x00,0x00,0x1A,
	0x00,0x00,0x00,0xFF,0x00,0x37,0x4A,0x51,
	0x58,0x42,0x59,0x32,0x0A,0x20,0x20,0x20,
	0x20,0x20,0x00,0x00,0x00,0xFC,0x00,0x53,
	0x32,0x37,0x31,0x39,0x44,0x47,0x46,0x0A,
	0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFD,
	0x00,0x28,0x9B,0xFA,0xFA,0x40,0x01,0x0A,
	0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x2C,
};

NTSTATUS Filter_HandleDesiredVidPn(
	PWDDM_ADAPTER wddmAdapter,
	INOUT_PDXGKARG_ISSUPPORTEDVIDPN IsSupportedVidPn
);

NTSTATUS Filter_UpdateConstrainingVidPn(
	PWDDM_ADAPTER wddmAdapter, 
	IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST pEnumCofuncModality, 
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DKMDT_HVIDPNTOPOLOGY hTopology, 
	const DXGK_VIDPNTOPOLOGY_INTERFACE* TopologyInterface, 
	const D3DKMDT_VIDPN_PRESENT_PATH* pathInfo
);

NTSTATUS Filter_HackConstrainingVidPn(
	PWDDM_ADAPTER wddmAdapter, 
	IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST EnumCofuncModality
);


NTSTATUS Filter_CreateSourceModeSetForTarget(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
	D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId
);

NTSTATUS Filter_CreateTargetModeSet(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnTargetId
);

WDDM_HOOK_GLOBAL Global;

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
		wf->DriverObject,
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


NTSTATUS create_wddm_filter_ctrl_device(PDRIVER_OBJECT DriverObject)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG IoControlCode;
	ULONG returnLength;

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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	pr_debug("-> Filter_DxgkDdiAddDevice()\n");

	wddmAdapter = WddmHookFindAdapterFromPdo(PhysicalDeviceObject);
	ASSERT(wddmAdapter);
	ASSERT(wddmAdapter->MiniportDeviceContext == NULL);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiAddDevice);

	status = DriverInitData->DxgkDdiAddDevice(PhysicalDeviceObject, MiniportDeviceContext);

	if (NT_SUCCESS(status)) {
		pr_debug("    MiniportDeviceContext: %p\n", *MiniportDeviceContext);
		wddmAdapter->MiniportDeviceContext = *MiniportDeviceContext;
	}

	pr_debug("<- Filter_DxgkDdiAddDevice(), status(0x%08x)\n", status);
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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	pr_debug("-> Filter_DxgkDdiStartDevice()\n");

	wddmAdapter = WddmHookFindAdapterFromContext(MiniportDeviceContext);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiStartDevice);

	DxgkInterface = Filter_SetupDxgkInterfaceHook(wddmAdapter, DxgkInterface);

	status = DriverInitData->DxgkDdiStartDevice(
		wddmAdapter->MiniportDeviceContext,
		DxgkStartInfo,
		DxgkInterface,
		NumberOfVideoPresentSources,
		NumberOfChildren
	);

	if (NT_SUCCESS(status)) {
		wddmAdapter->NumberOfVideoPresentSources = *NumberOfVideoPresentSources;
		wddmAdapter->NumberOfChildren = *NumberOfChildren;

		pr_debug("    NumberOfVideoPresentSources: %d\n", *NumberOfVideoPresentSources);
		pr_debug("    NumberOfChildren: %d\n", *NumberOfChildren);

		if (Global.fEnumVirtualChild) {
			*NumberOfChildren = *NumberOfChildren + 1;
		}

		KeSetTimer(
			&wddmAdapter->timer,
			wddmAdapter->timerDueTime,
			&wddmAdapter->timerDpc
		);
	}

	pr_debug("<- Filter_DxgkDdiStartDevice(), status(0x%08x)\n", status);

	return status;
}


NTSTATUS
Filter_DxgkDdiStopDevice(
	IN_CONST_PVOID  MiniportDeviceContext
)
{
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(MiniportDeviceContext);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiStopDevice);

	pr_debug("-> Filter_DxgkDdiStopDevice()\n");

	if (KeCancelTimer(&wddmAdapter->timer)) {
		KeFlushQueuedDpcs();
	}

	status = DriverInitData->DxgkDdiStopDevice(
		wddmAdapter->MiniportDeviceContext
	);

	pr_debug("<- Filter_DxgkDdiStopDevice(), status(0x%08x)\n", status);

	return status;
}


NTSTATUS
Filter_DxgkDdiRemoveDevice(
	IN_CONST_PVOID  MiniportDeviceContext
)
{
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;
	KIRQL oldIrql;

	pr_debug("-> Filter_DxgkDdiRemoveDevice()\n");

	wddmAdapter = WddmHookFindAdapterFromContext(MiniportDeviceContext);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	KeAcquireSpinLock(&wddmAdapter->WddmDriver->Lock, &oldIrql);
	RemoveEntryList(&wddmAdapter->List);
	KeReleaseSpinLock(&wddmAdapter->WddmDriver->Lock, oldIrql);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiRemoveDevice);

	status = DriverInitData->DxgkDdiRemoveDevice(
		wddmAdapter->MiniportDeviceContext
	);

	_FREE(wddmAdapter);

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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;
	PDXGK_CHILD_DESCRIPTOR origRelations = NULL;
	ULONG origRelationsSize;

	wddmAdapter = WddmHookFindAdapterFromContext(MiniportDeviceContext);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiQueryChildRelations);

	pr_debug("-> Filter_DxgkDdiQueryChildRelations()\n");

	origRelationsSize = (wddmAdapter->NumberOfChildren + 1) * sizeof(DXGK_CHILD_DESCRIPTOR);
	origRelations = (PDXGK_CHILD_DESCRIPTOR)_ALLOC(NonPagedPool, origRelationsSize, WDDM_HOOK_MEMORY_TAG);

	if (!origRelations) {
		pr_err("allocate DXGK_CHILD_DESCRIPTOR failed\n");
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto End;
	}

	RtlZeroMemory(origRelations, origRelationsSize);

	status = DriverInitData->DxgkDdiQueryChildRelations(
		wddmAdapter->MiniportDeviceContext,
		origRelations,
		origRelationsSize
	);

	if (NT_SUCCESS(status)) {
		DWORD n = origRelationsSize / sizeof(DXGK_CHILD_DESCRIPTOR);

		// get rid of trailing empty ChildDescriptor
		n--;

		RtlCopyMemory(
			ChildRelations,
			origRelations,
			n * sizeof(DXGK_CHILD_DESCRIPTOR)
		);

		if (Global.fEnumVirtualChild) {
			RtlCopyMemory(
				&ChildRelations[n],
				&wddmAdapter->VirtualChild.Descriptor,
				sizeof(DXGK_CHILD_DESCRIPTOR)
			);
			n++;
		}

		TraceIndent();
		Dump_DXGK_CHILD_DESCRIPTOR(ChildRelations, n);
		TraceUnindent();
	}

End:
	if (origRelations) {
		_FREE(origRelations);
	}

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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(MiniportDeviceContext);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiQueryChildStatus);

	pr_debug("-> Filter_DxgkDdiQueryChildStatus(), NonDestructiveOnly(%d)\n",
		NonDestructiveOnly);

	status = DriverInitData->DxgkDdiQueryChildStatus(
		wddmAdapter->MiniportDeviceContext,
		ChildStatus,
		NonDestructiveOnly
	);

	if (NT_SUCCESS(status)) {
		TraceIndent();
		Dump_DXGK_CHILD_STATUS(ChildStatus);
		TraceUnindent();
	}
	
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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(MiniportDeviceContext);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiQueryDeviceDescriptor);

	pr_debug("-> Filter_DxgkDdiQueryDeviceDescriptor(), ChildUid(%d)\n",
		ChildUid);

	if (ChildUid == wddmAdapter->VirtualChild.Descriptor.ChildUid) {
		if (DeviceDescriptor->DescriptorOffset + DeviceDescriptor->DescriptorLength > sizeof(SampleEDID)) {
			status = STATUS_MONITOR_NO_DESCRIPTOR;
		}
		else {
			RtlCopyMemory(
				DeviceDescriptor->DescriptorBuffer,
				&SampleEDID[DeviceDescriptor->DescriptorOffset],
				DeviceDescriptor->DescriptorLength
			);
			status = STATUS_SUCCESS;
		}
	}
	else {
		status = DriverInitData->DxgkDdiQueryDeviceDescriptor(
			wddmAdapter->MiniportDeviceContext,
			ChildUid,
			DeviceDescriptor
		);
	}

	if (NT_SUCCESS(status)) {
		Dump_DXGK_DEVICE_DESCRIPTOR(DeviceDescriptor);
	}

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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiIsSupportedVidPn);

	pr_debug("-> Filter_DxgkDdiIsSupportedVidPn()\n");

	Dump_VidPn(wddmAdapter, pIsSupportedVidPn->hDesiredVidPn);

	status = Filter_HandleDesiredVidPn(wddmAdapter, pIsSupportedVidPn);
	if (status == STATUS_NOT_FOUND) {
		status = DriverInitData->DxgkDdiIsSupportedVidPn(
			wddmAdapter->MiniportDeviceContext,
			pIsSupportedVidPn
		);
	}

	if (NT_SUCCESS(status)) {
		pr_info("    IsVidPnSupported: %d\n", pIsSupportedVidPn->IsVidPnSupported);
	}
	
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
	NTSTATUS status;
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	const DXGK_VIDPN_INTERFACE* VidPnInterface = NULL;
	D3DKMDT_HVIDPNTOPOLOGY hTopology = NULL;
	const DXGK_VIDPNTOPOLOGY_INTERFACE* topologyInterface = NULL;
	D3DDDI_VIDEO_PRESENT_SOURCE_ID sourceId = -1;
	const D3DKMDT_VIDPN_PRESENT_PATH* pathInfo = NULL;
	SIZE_T pathnum;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiEnumVidPnCofuncModality);

	pr_debug("-> Filter_DxgkDdiEnumVidPnCofuncModality()\n");
	TraceIndent();

	pr_info("original constraining VidPn\n");
	Dump_DXGKARG_ENUMVIDPNCOFUNCMODALITY(wddmAdapter, pEnumCofuncModality);

#if 1
	goto ByPass;

	status = Filter_GetVidPnTopology(
		wddmAdapter,
		pEnumCofuncModality->hConstrainingVidPn,
		&VidPnInterface,
		&hTopology,
		&topologyInterface
	);
	if (status != STATUS_SUCCESS) {
		goto Cleanup;
	}

	status = topologyInterface->pfnGetPathSourceFromTarget(
		hTopology, wddmAdapter->VirtualChild.Descriptor.ChildUid, &sourceId
	);
	if (status != STATUS_SUCCESS || sourceId == -1) {
		goto ByPass;
	}

	status = topologyInterface->pfnAcquirePathInfo(
		hTopology,
		sourceId,
		wddmAdapter->VirtualChild.Descriptor.ChildUid,
		&pathInfo
	);
	if (status != STATUS_SUCCESS) {
		pr_err("acquire path source(%d) target(%d) failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	pr_info("found %d target\n", 
		wddmAdapter->VirtualChild.Descriptor.ChildUid);

	/*
	status = topologyInterface->pfnRemovePath(
		hTopology, sourceId, wddmAdapter->VirtualChild.Descriptor.ChildUid
	);
	if (status != STATUS_SUCCESS) {
		pr_err("remove path source(%d) target(%d) failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	status = topologyInterface->pfnGetNumPaths(
		hTopology, &pathnum
	);
	if (status != STATUS_SUCCESS) {
		pr_err("get other path failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	if (pathnum > 0) {
		status = DriverInitData->DxgkDdiEnumVidPnCofuncModality(
			wddmAdapter->MiniportDeviceContext,
			pEnumCofuncModality
		);

		if (status != STATUS_SUCCESS) {
			goto Cleanup;
		}
	}
	*/

	status = Filter_UpdateConstrainingVidPn(
		wddmAdapter, 
		pEnumCofuncModality, 
		VidPnInterface,
		hTopology, 
		topologyInterface, 
		pathInfo
	);
	if (status != STATUS_SUCCESS) {
		goto Cleanup;
	}

	goto Cleanup;
#else
	status = Filter_HackConstrainingVidPn(wddmAdapter, pEnumCofuncModality);
	if (status != STATUS_SUCCESS) {
		goto Cleanup;
	}

	goto ByPass;
#endif 

ByPass:
	status = DriverInitData->DxgkDdiEnumVidPnCofuncModality(
		wddmAdapter->MiniportDeviceContext,
		pEnumCofuncModality
	);

Cleanup:

	if (NT_SUCCESS(status)) {
		pr_info("result constraining VidPn\n");
		Dump_DXGKARG_ENUMVIDPNCOFUNCMODALITY(wddmAdapter, pEnumCofuncModality);
	}

	if (pathInfo) {
		ASSERT(hTopology);
		ASSERT(topologyInterface);

		topologyInterface->pfnReleasePathInfo(hTopology, pathInfo);
	}

	TraceUnindent();
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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiSetVidPnSourceAddress);

	pr_debug("-> Filter_DxgkDdiSetVidPnSourceAddress()\n");

	status = DriverInitData->DxgkDdiSetVidPnSourceAddress(
		wddmAdapter->MiniportDeviceContext,
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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiSetVidPnSourceVisibility);

	pr_debug("-> Filter_DxgkDdiSetVidPnSourceVisibility()\n"
		"    VidPnSourceId(%d), Visible(%d)\n",
		pSetVidPnSourceVisibility->VidPnSourceId, 
		pSetVidPnSourceVisibility->Visible);

	status = DriverInitData->DxgkDdiSetVidPnSourceVisibility(
		wddmAdapter->MiniportDeviceContext,
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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiCommitVidPn);

	pr_debug("-> Filter_DxgkDdiCommitVidPn()\n");
	TraceIndent();
	Dump_DXGKARG_COMMITVIDPN(wddmAdapter, pCommitVidPn);

	status = DriverInitData->DxgkDdiCommitVidPn(
		wddmAdapter->MiniportDeviceContext,
		pCommitVidPn
	);

	TraceUnindent();

	pr_debug("<- Filter_DxgkDdiCommitVidPn(), status(0x%08x)\n", status);

	return status;
}

NTSTATUS
APIENTRY
Filter_DxgkDdiRecommendMonitorModes(
	IN_CONST_HANDLE                                 hAdapter,
	IN_CONST_PDXGKARG_RECOMMENDMONITORMODES_CONST   pRecommendMonitorModes
)
{
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiRecommendMonitorModes);

	pr_debug("-> Filter_DxgkDdiRecommendMonitorModes()\n");

	pr_debug("original monitors\n");
	Dump_DXGKARG_RECOMMENDMONITORMODES(wddmAdapter, pRecommendMonitorModes);

	status = DriverInitData->DxgkDdiRecommendMonitorModes(
		wddmAdapter->MiniportDeviceContext,
		pRecommendMonitorModes
	);

	if (status == STATUS_SUCCESS) {
		pr_debug("modified monitors\n");
		Dump_DXGKARG_RECOMMENDMONITORMODES(wddmAdapter, pRecommendMonitorModes);
	}

	pr_debug("<- Filter_DxgkDdiRecommendMonitorModes(), status(0x%08x)\n", status);

	return status;
}

NTSTATUS
APIENTRY
Filter_DxgkDdiQueryConnectionChange(
	IN_CONST_HANDLE                         hAdapter,
	IN_PDXGKARG_QUERYCONNECTIONCHANGE       pQueryConnectionChange
)
{
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	DXGK_CONNECTION_CHANGE* connectionChange;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiQueryConnectionChange);

	pr_debug("-> Filter_DxgkDdiQueryconnectionchange()\n");

	if (wddmAdapter->VirtualChild.updatePending) {
		connectionChange = &pQueryConnectionChange->ConnectionChange;
		connectionChange->ConnectionChangeId = InterlockedIncrement(&wddmAdapter->VirtualChild.connectionChangeId);
		connectionChange->TargetId = wddmAdapter->VirtualChild.Descriptor.ChildUid;
		connectionChange->ConnectionStatus = MonitorStatusConnected;
		connectionChange->MonitorConnect.LinkTargetType = D3DKMDT_VOT_HDMI;

		wddmAdapter->VirtualChild.updatePending = FALSE;

		status = STATUS_SUCCESS;
	}
	else {
		status = DriverInitData->DxgkDdiQueryConnectionChange(hAdapter, pQueryConnectionChange);
	}
	
	if (status == STATUS_SUCCESS) {
		Dump_DXGKARG_QUERYCONNECTIONCHANGE(wddmAdapter, pQueryConnectionChange);
	}

	pr_debug("<- Filter_DxgkDdiQueryConnectionChange(), status(0x%08x)\n", status);

	return status;
}

PWDDM_DRIVER WddmDriverAlloc(PDRIVER_OBJECT DriverObject)
{
	PWDDM_DRIVER wddmDriver;

	wddmDriver = (PWDDM_DRIVER)_ALLOC(
		NonPagedPool, sizeof(*wddmDriver), WDDM_HOOK_MEMORY_TAG
	);
	if (!wddmDriver) {
		pr_err("allocate WDDM_DRIVER failed\n");
		return NULL;
	}

	RtlZeroMemory(wddmDriver, sizeof(*wddmDriver));

	SIGNATURE_ASSIGN(wddmDriver->Signature, WDDM_DRIVER_SIGNATURE);
	InitializeListHead(&wddmDriver->List);
	wddmDriver->DriverObject = DriverObject;
	KeInitializeSpinLock(&wddmDriver->Lock);
	InitializeListHead(&wddmDriver->AdapterList);

	return wddmDriver;
}

PWDDM_ADAPTER WddmAdapterAlloc(PWDDM_DRIVER WddmDriver)
{
	PWDDM_ADAPTER wddmAdapter;
	DXGK_CHILD_DESCRIPTOR* childDescriptor;

	wddmAdapter = (PWDDM_ADAPTER)_ALLOC(
		NonPagedPool, sizeof(*wddmAdapter), WDDM_HOOK_MEMORY_TAG
	);
	if (!wddmAdapter) {
		pr_err("alloc WDDM_ADAPTER failed\n");
		return NULL;
	}

	RtlZeroMemory(wddmAdapter, sizeof(*wddmAdapter));

	SIGNATURE_ASSIGN(wddmAdapter->Signature, WDDM_ADAPTER_SIGNATURE);
	InitializeListHead(&wddmAdapter->List);
	wddmAdapter->WddmDriver = WddmDriver;
	
	KeInitializeTimer(&wddmAdapter->timer);
	KeInitializeDpc(&wddmAdapter->timerDpc, Win10WddmAdapterTimerRoutine, wddmAdapter);
	wddmAdapter->timerDueTime.QuadPart = -1 * 10 * 1000 * 1000 * 2; // 2 seconds
	wddmAdapter->indicateVirtualDisplay = FALSE;

	childDescriptor = &wddmAdapter->VirtualChild.Descriptor;

	// virtual child is a HDMI connector
	//
	childDescriptor->ChildDeviceType = TypeVideoOutput;
	childDescriptor->ChildCapabilities.Type.VideoOutput.InterfaceTechnology = D3DKMDT_VOT_HDMI;
	childDescriptor->ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness = D3DKMDT_MOA_NONE;
	childDescriptor->ChildCapabilities.Type.VideoOutput.SupportsSdtvModes = FALSE;
	childDescriptor->ChildCapabilities.HpdAwareness = HpdAwarenessNone;
	childDescriptor->AcpiUid = 0;
	childDescriptor->ChildUid = WDDM_HOOK_VIRTUAL_HDMI_CHILD_UID;

	wddmAdapter->VirtualChild.updatePending = FALSE;

	return wddmAdapter;
}


PWDDM_DRIVER WddmHookFindDriver(PDRIVER_OBJECT DriverObject)
{
	KIRQL OldIrql;
	PLIST_ENTRY Entry;
	PWDDM_DRIVER wddmdriver = NULL;

	KeAcquireSpinLock(&Global.Lock, &OldIrql);

	Entry = Global.HookDriverList.Flink;
	while (Entry != &Global.HookDriverList) {
		wddmdriver = CONTAINING_RECORD(Entry, WDDM_DRIVER, List);
		if (wddmdriver->DriverObject == DriverObject)
			break;

		wddmdriver = NULL;
		Entry = Entry->Flink;
	}

	KeReleaseSpinLock(&Global.Lock, OldIrql);

	return wddmdriver;
}


PWDDM_ADAPTER WddmDriverFindAdapter(
	PWDDM_DRIVER WddmDriver,
	PWDDM_ADAPTER_FIND_DATA FindData
)
{
	KIRQL OldIrql;
	PLIST_ENTRY Entry;
	PWDDM_ADAPTER wddmAdapter = NULL;

	KeAcquireSpinLock(&WddmDriver->Lock, &OldIrql);

	Entry = WddmDriver->AdapterList.Flink;
	while (Entry != &WddmDriver->AdapterList) {
		wddmAdapter = CONTAINING_RECORD(Entry, WDDM_ADAPTER, List);

		ASSERT(wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);

		if (FindData->Flags & WDDM_HOOK_FIND_FIRST_ADAPTER) {
			break;
		}

		if ((FindData->Flags & WDDM_HOOK_FIND_MATCH_PDO) != 0 &&
			FindData->PhysicalDeviceObject == wddmAdapter->PhysicalDeviceObject)
		{
			break;
		}

		if ((FindData->Flags & WDDM_HOOK_FIND_MATCH_DEVICE_CONTEXT) != 0 &&
			FindData->MiniportDeviceContext == wddmAdapter->MiniportDeviceContext)
		{
			break;
		}

		if ((FindData->Flags & WDDM_HOOK_FIND_MATCH_DEVICE_HANDLE) != 0 &&
			FindData->DeviceHandle == wddmAdapter->DxgkInterface.DeviceHandle)
		{
			break;
		}

		wddmAdapter = NULL;
		Entry = Entry->Flink;
	}

	KeReleaseSpinLock(&WddmDriver->Lock, OldIrql);

	return wddmAdapter;
}

PWDDM_ADAPTER WddmHookFindAdapter(PWDDM_ADAPTER_FIND_DATA FindData)
{
	KIRQL OldIrql;
	PLIST_ENTRY Entry;
	PWDDM_DRIVER wddmDriver = NULL;
	PWDDM_ADAPTER wddmAdapter = NULL;

	KeAcquireSpinLock(&Global.Lock, &OldIrql);

	Entry = Global.HookDriverList.Flink;
	while (Entry != &Global.HookDriverList) {
		wddmDriver = CONTAINING_RECORD(Entry, WDDM_DRIVER, List);

		ASSERT(wddmDriver->Signature == WDDM_DRIVER_SIGNATURE);
		
		wddmAdapter = WddmDriverFindAdapter(wddmDriver, FindData);
		if (wddmAdapter) {
			break;
		}

		Entry = Entry->Flink;
	}

	KeReleaseSpinLock(&Global.Lock, OldIrql);

	return wddmAdapter;
}

PWDDM_ADAPTER WddmHookFindFirstAdapter()
{
	WDDM_ADAPTER_FIND_DATA findData = {
		.Flags = WDDM_HOOK_FIND_FIRST_ADAPTER,
	};

	return WddmHookFindAdapter(&findData);
}

PWDDM_ADAPTER WddmHookFindAdapterFromPdo(PDEVICE_OBJECT PhysicalDeviceObject)
{
	WDDM_ADAPTER_FIND_DATA findData = {
		.Flags = WDDM_HOOK_FIND_MATCH_PDO,
		.PhysicalDeviceObject = PhysicalDeviceObject
	};

	return WddmHookFindAdapter(&findData);
}

PWDDM_ADAPTER WddmHookFindAdapterFromContext(PVOID MiniportDeviceContext)
{
	WDDM_ADAPTER_FIND_DATA findData = {
		.Flags = WDDM_HOOK_FIND_MATCH_DEVICE_CONTEXT,
		.MiniportDeviceContext = MiniportDeviceContext
	};

	return WddmHookFindAdapter(&findData);
}

PWDDM_ADAPTER WddmHookFindAdapterFromDeviceHandle(HANDLE DeviceHandle)
{
	WDDM_ADAPTER_FIND_DATA findData = {
		.Flags = WDDM_HOOK_FIND_MATCH_DEVICE_HANDLE,
		.DeviceHandle = DeviceHandle
	};
	
	return WddmHookFindAdapter(&findData);
}

NTSTATUS Filter_VidPnHasPinnedSource(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID SourceId
)
{
	// Get the source mode set for this SourceId
	D3DKMDT_HVIDPNSOURCEMODESET hSourceModeSet;
	const DXGK_VIDPNSOURCEMODESET_INTERFACE* sourceModeSetInterface;
	const D3DKMDT_VIDPN_SOURCE_MODE* pinnedSourceMode = NULL;

	NTSTATUS status = VidPnInterface->pfnAcquireSourceModeSet(
		hVidPn,
		SourceId,
		&hSourceModeSet,
		&sourceModeSetInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnAcquireSourceModeSet() failed, status(0x%08x)\n", status);
		return status;
	}

	status = sourceModeSetInterface->pfnAcquirePinnedModeInfo(
		hSourceModeSet, &pinnedSourceMode
	);
	if (status != STATUS_SUCCESS) {
		status = STATUS_NOT_FOUND;
		goto Cleanup;
	}

	sourceModeSetInterface->pfnReleaseModeInfo(hSourceModeSet, pinnedSourceMode);

Cleanup:
	return status;
}


NTSTATUS Filter_VidPnHasPinnedTarget(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID TargetId
)
{
	// get the target mode set for this TargetId
	D3DKMDT_HVIDPNTARGETMODESET hTargetModeSet;
	const DXGK_VIDPNTARGETMODESET_INTERFACE* targetModeSetInterface;
	const D3DKMDT_VIDPN_TARGET_MODE* pinnedTargetMode = NULL;

	NTSTATUS status = VidPnInterface->pfnAcquireTargetModeSet(
		hVidPn,
		TargetId,
		&hTargetModeSet,
		&targetModeSetInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnAcquireTargetModeSet() failed, status(0x%08x)\n", status);
		return status;
	}

	status = targetModeSetInterface->pfnAcquirePinnedModeInfo(
		hTargetModeSet, &pinnedTargetMode
	);
	if (status != STATUS_SUCCESS) {
		status = STATUS_NOT_FOUND;
		goto Cleanup;
	}

	targetModeSetInterface->pfnReleaseModeInfo(hTargetModeSet, pinnedTargetMode);

Cleanup:
	return status;
}

NTSTATUS Filter_CreateNewSourceModeSet(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID SourceId
)
{
	NTSTATUS status;
	D3DKMDT_HVIDPNSOURCEMODESET hSourceModeSet = NULL;
	const DXGK_VIDPNSOURCEMODESET_INTERFACE* sourceModeSetInterface = NULL;
	D3DKMDT_VIDPN_SOURCE_MODE* sourceMode = NULL;

	status = VidPnInterface->pfnCreateNewSourceModeSet(
		hVidPn,
		SourceId,
		&hSourceModeSet,
		&sourceModeSetInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("create new source mode set failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	// create new source mode and add it to the source mode set
	//
	
	status = sourceModeSetInterface->pfnCreateNewModeInfo(
		hSourceModeSet, &sourceMode
	);
	if (status != STATUS_SUCCESS) {
		pr_err("create new source mode failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	sourceMode->Type = D3DKMDT_RMT_GRAPHICS;
	sourceMode->Format.Graphics.PrimSurfSize.cx = 1024;
	sourceMode->Format.Graphics.PrimSurfSize.cy = 768;
	sourceMode->Format.Graphics.VisibleRegionSize.cx = 1024;
	sourceMode->Format.Graphics.VisibleRegionSize.cy = 768;
	sourceMode->Format.Graphics.Stride = 0x1000;
	sourceMode->Format.Graphics.PixelFormat = D3DDDIFMT_A8R8G8B8;
	sourceMode->Format.Graphics.ColorBasis = D3DKMDT_CB_SRGB;
	sourceMode->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_DIRECT;

	status = sourceModeSetInterface->pfnAddMode(hSourceModeSet, sourceMode);
	if (status != STATUS_SUCCESS) {
		pr_err("add source mode failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	// the added source mode is managed by source mode set
	// 
	sourceMode = NULL;

	// replace the original source mode set 
	//
	status = VidPnInterface->pfnAssignSourceModeSet(
		hVidPn,
		SourceId,
		hSourceModeSet
	);
	if (status != STATUS_SUCCESS) {
		pr_err("assign source mode set to source(%d) failed, status(0x%08x)\n",
			SourceId, status);
		goto Cleanup;
	}

	// the source mode set is managed by VidPn
	//
	hSourceModeSet = NULL;

Cleanup:
	if (sourceMode) {
		sourceModeSetInterface->pfnReleaseModeInfo(hSourceModeSet, sourceMode);
	}

	if (hSourceModeSet) {
		VidPnInterface->pfnReleaseSourceModeSet(hVidPn, hSourceModeSet);
	}

	return status;
}


VOID
Win10WddmAdapterTimerRoutine(
	_In_ struct _KDPC* Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
)
{
	PWDDM_ADAPTER wddmAdapter = (PWDDM_ADAPTER)DeferredContext;
	const DXGKRNL_INTERFACE* dxgkInterface;
	NTSTATUS status;

	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	if (wddmAdapter->indicateVirtualDisplay) {
		dxgkInterface = &wddmAdapter->DxgkInterface;
		ASSERT(dxgkInterface->DxgkCbIndicateConnectorChange);

		wddmAdapter->indicateVirtualDisplay = FALSE;
		wddmAdapter->VirtualChild.updatePending = TRUE;

		status = dxgkInterface->DxgkCbIndicateConnectorChange(
			dxgkInterface->DeviceHandle
		);

		pr_info("DxgkCbIndicateConnectorChange() return 0x%08x\n", status);
	}

	KeSetTimer(
		&wddmAdapter->timer, 
		wddmAdapter->timerDueTime, 
		&wddmAdapter->timerDpc
	);
}


NTSTATUS Filter_HandleDesiredVidPn(
	PWDDM_ADAPTER WddmAdapter, 
	INOUT_PDXGKARG_ISSUPPORTEDVIDPN IsSupportedVidPn
)
{
	PDXGKRNL_INTERFACE dxgkInterface = &WddmAdapter->DxgkInterface;
	const DXGK_VIDPN_INTERFACE* VidPnInterface = NULL;
	D3DKMDT_HVIDPNTOPOLOGY hTopology = NULL;
	DXGK_VIDPNTOPOLOGY_INTERFACE* topologyInterface = NULL;
	D3DKMDT_VIDPN_PRESENT_PATH* pathInfo = NULL;
	SIZE_T pathnum = 0;
	NTSTATUS status;

	if (IsSupportedVidPn->hDesiredVidPn == NULL) {
		return STATUS_NOT_FOUND;
	}

	status = dxgkInterface->DxgkCbQueryVidPnInterface(
		IsSupportedVidPn->hDesiredVidPn, 
		DXGK_VIDPN_INTERFACE_VERSION_V1, 
		&VidPnInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("DxgkCbQueryVidPnInterface() for hDesiredVidPn failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	status = VidPnInterface->pfnGetTopology(
		IsSupportedVidPn->hDesiredVidPn,
		&hTopology,
		&topologyInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("get hDesiredVidPn topolgy failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	status = topologyInterface->pfnGetNumPaths(hTopology, &pathnum);
	if (status != STATUS_SUCCESS) {
		pr_err("get hDesiredVidPn topology path number failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	pr_info("hDesiredVidPn Path Number: %d\n", pathnum);

	if (pathnum != 1) {
		status = STATUS_NOT_FOUND;
		goto Cleanup;
	}

	status = topologyInterface->pfnAcquireFirstPathInfo(hTopology, &pathInfo);
	if (status != STATUS_SUCCESS) {
		pr_err("acquire hDesiredVidPn first path failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	if (pathInfo->VidPnTargetId != WddmAdapter->VirtualChild.Descriptor.ChildUid) {
		status = STATUS_NOT_FOUND;
		goto Cleanup;
	}

	IsSupportedVidPn->IsVidPnSupported = TRUE;
	status = STATUS_SUCCESS;

Cleanup:
	if (pathInfo != NULL) {
		ASSERT(topologyInterface);
		topologyInterface->pfnReleasePathInfo(hTopology, pathInfo);
	}

	return status;
}

NTSTATUS Filter_UpdateConstrainingVidPn(
	PWDDM_ADAPTER wddmAdapter,
	IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST EnumCofuncModality,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DKMDT_HVIDPNTOPOLOGY hTopology,
	const DXGK_VIDPNTOPOLOGY_INTERFACE* TopologyInterface,
	const D3DKMDT_VIDPN_PRESENT_PATH* pathInfo
)
{
	NTSTATUS status;
	// D3DKMDT_VIDPN_PRESENT_PATH* newPath = NULL;
	D3DKMDT_VIDPN_PRESENT_PATH modifiedPath;
	BOOLEAN presentPathModified = FALSE;

	/*
	status = TopologyInterface->pfnCreateNewPathInfo(hTopology, &newPath);
	if (status != STATUS_SUCCESS) {
		pr_err("create new pathinfo failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	RtlCopyMemory(newPath, pathInfo, sizeof(*pathInfo));

	status = TopologyInterface->pfnAddPath(hTopology, newPath);
	if (status != STATUS_SUCCESS) {
		pr_info("add newPath failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	pathInfo = newPath;
	newPath = NULL;
	*/

	// If this source mode set isn't the pivot point, inspect the source
	// mode set and potentially add cofunctional modes
	if (!((EnumCofuncModality->EnumPivotType == D3DKMDT_EPT_VIDPNSOURCE) &&
		  (EnumCofuncModality->EnumPivot.VidPnSourceId == pathInfo->VidPnSourceId)))
	{
		status = Filter_VidPnHasPinnedSource(
			EnumCofuncModality->hConstrainingVidPn,
			VidPnInterface,
			pathInfo->VidPnSourceId
		);
		switch (status) {
		case STATUS_SUCCESS: break;
		case STATUS_NOT_FOUND:
			status = Filter_CreateSourceModeSetForTarget(
				EnumCofuncModality->hConstrainingVidPn,
				VidPnInterface,
				pathInfo->VidPnSourceId,
				pathInfo->VidPnTargetId
			);
			if (status != STATUS_SUCCESS) {
				goto Cleanup;
			}
			break;
		default:
			goto Cleanup;
		}
	}

	// If this target mode set isn't the pivot point, inspect the target
	// mode set and potentially add cofunctional modes
	if (!((EnumCofuncModality->EnumPivotType == D3DKMDT_EPT_VIDPNTARGET) &&
		  (EnumCofuncModality->EnumPivot.VidPnTargetId == pathInfo->VidPnTargetId)))
	{
		status = Filter_VidPnHasPinnedTarget(
			EnumCofuncModality->hConstrainingVidPn,
			VidPnInterface,
			pathInfo->VidPnTargetId
		);

		switch (status) {
		case STATUS_SUCCESS: break;
		case STATUS_NOT_FOUND:
			status = Filter_CreateTargetModeSet(
				EnumCofuncModality->hConstrainingVidPn,
				VidPnInterface,
				pathInfo->VidPnTargetId
			);
			if (status != STATUS_SUCCESS) {
				goto Cleanup;
			}
			break;
		default:
			goto Cleanup;
		}
	} // target mode set

	modifiedPath = *pathInfo;

	// SCALING: If this path's scaling isn't the pivot point, do work on the scaling support
	if (!((EnumCofuncModality->EnumPivotType == D3DKMDT_EPT_SCALING) &&
		  (EnumCofuncModality->EnumPivot.VidPnSourceId == pathInfo->VidPnSourceId) &&
		  (EnumCofuncModality->EnumPivot.VidPnTargetId == pathInfo->VidPnTargetId)))
	{
		// If the scaling is unpinned, then modify the scaling support field
		if (pathInfo->ContentTransformation.Scaling == D3DKMDT_VPPS_UNPINNED)
		{
			// Identity and centered scaling are supported, but not any stretch modes
			RtlZeroMemory(
				&modifiedPath.ContentTransformation.ScalingSupport,
				sizeof(D3DKMDT_VIDPN_PRESENT_PATH_SCALING_SUPPORT)
			);

			// We do not support scaling
			modifiedPath.ContentTransformation.ScalingSupport.Identity = TRUE;

			presentPathModified = TRUE;
		}
	} // scaling

	// ROTATION: If this path's rotation isn't the pivot point, do work on the rotation support
	if (!((EnumCofuncModality->EnumPivotType != D3DKMDT_EPT_ROTATION) &&
	  	  (EnumCofuncModality->EnumPivot.VidPnSourceId == pathInfo->VidPnSourceId) &&
		  (EnumCofuncModality->EnumPivot.VidPnTargetId == pathInfo->VidPnTargetId)))
	{
		// If the rotation is unpinned, then modify the rotation support field
		if (pathInfo->ContentTransformation.Rotation == D3DKMDT_VPPR_UNPINNED)
		{
			RtlZeroMemory(
				&modifiedPath.ContentTransformation.RotationSupport,
				sizeof(D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_SUPPORT)
			);

			// We do not support rotation
			modifiedPath.ContentTransformation.RotationSupport.Identity = TRUE;
			modifiedPath.ContentTransformation.RotationSupport.Offset0 = TRUE;

			presentPathModified = TRUE;
		}
	} // rotation

	// Update the content transformation
	if (presentPathModified) {
		// The correct path will be found by this function and the appropriate fields updated
		status = TopologyInterface->pfnUpdatePathSupportInfo(
			hTopology,
			&modifiedPath
		);
		if (status != STATUS_SUCCESS) {
			goto Cleanup;
		}
	}

	pathInfo = NULL;

Cleanup:

	return status;
}

NTSTATUS Filter_HackConstrainingVidPn(
	PWDDM_ADAPTER WddmAdapter,
	IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST EnumCofuncModality
)
{
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;
	const DXGK_VIDPN_INTERFACE* VidPnInterface;
	D3DKMDT_HVIDPNTOPOLOGY hTopology;
	const DXGK_VIDPNTOPOLOGY_INTERFACE* topologyInterface;
	D3DDDI_VIDEO_PRESENT_SOURCE_ID sourceId;
	D3DDDI_VIDEO_PRESENT_TARGET_ID targetId;
	const D3DKMDT_VIDPN_PRESENT_PATH* pathInfo = NULL;
	D3DKMDT_VIDPN_PRESENT_PATH modifiedPath;
	BOOLEAN presentPathModified = FALSE;

	targetId = WddmAdapter->VirtualChild.Descriptor.ChildUid;

	status = Filter_GetVidPnTopology(
		WddmAdapter,
		EnumCofuncModality->hConstrainingVidPn,
		&VidPnInterface,
		&hTopology,
		&topologyInterface
	);
	if (status != STATUS_SUCCESS) {
		goto Cleanup;
	}

	status = topologyInterface->pfnGetPathSourceFromTarget(
		hTopology, targetId, &sourceId
	);
	if (status != STATUS_SUCCESS) {
		status = STATUS_SUCCESS;
		goto Cleanup;
	}

	status = topologyInterface->pfnAcquirePathInfo(
		hTopology, sourceId, targetId, &pathInfo
	);
	if (status != STATUS_SUCCESS) {
		pr_err("acquire path source(%d) target(%d) failed, status(0x%08x)\n", 
			sourceId, targetId, status);
		goto Cleanup;
	}

	// If this source mode set isn't the pivot point, inspect the source
	// mode set and potentially add cofunctional modes
	if (!((EnumCofuncModality->EnumPivotType == D3DKMDT_EPT_VIDPNSOURCE) &&
		  (EnumCofuncModality->EnumPivot.VidPnSourceId == pathInfo->VidPnSourceId)))
	{
		status = Filter_VidPnHasPinnedSource(
			EnumCofuncModality->hConstrainingVidPn,
			VidPnInterface,
			sourceId
		);
		switch (status) {
		case STATUS_SUCCESS: break;
		case STATUS_NOT_FOUND:
			status = Filter_CreateSourceModeSetForTarget(
				EnumCofuncModality->hConstrainingVidPn,
				VidPnInterface,
				sourceId,
				targetId
			);
			if (status != STATUS_SUCCESS) {
				goto Cleanup;
			}
			break;
		default:
			goto Cleanup;
		}
	} 

	// If this target mode set isn't the pivot point, inspect the target
	// mode set and potentially add cofunctional modes
	if (!((EnumCofuncModality->EnumPivotType == D3DKMDT_EPT_VIDPNTARGET) &&
		  (EnumCofuncModality->EnumPivot.VidPnTargetId == pathInfo->VidPnTargetId)))
	{
		status = Filter_VidPnHasPinnedTarget(
			EnumCofuncModality->hConstrainingVidPn,
			VidPnInterface,
			targetId
		);

		switch (status) {
		case STATUS_SUCCESS: break;
		case STATUS_NOT_FOUND:
			status = Filter_CreateTargetModeSet(
				EnumCofuncModality->hConstrainingVidPn,
				VidPnInterface,
				targetId
			);
			if (status != STATUS_SUCCESS) {
				goto Cleanup;
			}
			break;
		default:
			goto Cleanup;
		}
	} // target mode set

	modifiedPath = *pathInfo;

	// SCALING: If this path's scaling isn't the pivot point, do work on the scaling support
	if (!((EnumCofuncModality->EnumPivotType == D3DKMDT_EPT_SCALING) &&
		  (EnumCofuncModality->EnumPivot.VidPnSourceId == pathInfo->VidPnSourceId) &&
		  (EnumCofuncModality->EnumPivot.VidPnTargetId == pathInfo->VidPnTargetId)))
	{
		// If the scaling is unpinned, then modify the scaling support field
		if (pathInfo->ContentTransformation.Scaling == D3DKMDT_VPPS_UNPINNED)
		{
			// Identity and centered scaling are supported, but not any stretch modes
			RtlZeroMemory(
				&modifiedPath.ContentTransformation.ScalingSupport,
				sizeof(D3DKMDT_VIDPN_PRESENT_PATH_SCALING_SUPPORT)
			);

			// We do not support scaling
			modifiedPath.ContentTransformation.ScalingSupport.Identity = TRUE;

			presentPathModified = TRUE;
		}
	} // scaling

	// ROTATION: If this path's rotation isn't the pivot point, do work on the rotation support
	if (!((EnumCofuncModality->EnumPivotType != D3DKMDT_EPT_ROTATION) &&
		  (EnumCofuncModality->EnumPivot.VidPnSourceId == pathInfo->VidPnSourceId) &&
		  (EnumCofuncModality->EnumPivot.VidPnTargetId == pathInfo->VidPnTargetId)))
	{
		// If the rotation is unpinned, then modify the rotation support field
		if (pathInfo->ContentTransformation.Rotation == D3DKMDT_VPPR_UNPINNED)
		{
			RtlZeroMemory(
				&modifiedPath.ContentTransformation.RotationSupport,
				sizeof(D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_SUPPORT)
			);

			// We do not support rotation
			modifiedPath.ContentTransformation.RotationSupport.Identity = TRUE;
			modifiedPath.ContentTransformation.RotationSupport.Offset0 = TRUE;

			presentPathModified = TRUE;
		}
	} // rotation

	// Update the content transformation
	if (presentPathModified) {
		// The correct path will be found by this function and the appropriate fields updated
		status = topologyInterface->pfnUpdatePathSupportInfo(
			hTopology,
			&modifiedPath
		);
		if (status != STATUS_SUCCESS) {
			goto Cleanup;
		}
	}
	
Cleanup:
	if (pathInfo) {
		ASSERT(hTopology);
		ASSERT(topologyInterface);

		topologyInterface->pfnReleasePathInfo(hTopology, pathInfo);
	}

	return status;
}

NTSTATUS Filter_CreateSourceModeSetForTarget(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
	D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId
)
{
	NTSTATUS status;
	D3DKMDT_HVIDPNSOURCEMODESET hSourceModeSet = NULL;
	const DXGK_VIDPNSOURCEMODESET_INTERFACE* sourceModeSetInterface;
	D3DKMDT_VIDPN_SOURCE_MODE* sourcemode = NULL;

	UNREFERENCED_PARAMETER(VidPnTargetId);

	status = VidPnInterface->pfnCreateNewSourceModeSet(
		hVidPn, VidPnSourceId, &hSourceModeSet, &sourceModeSetInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("create source mode set for source(%d) failed, status(0x%08x)\n",
			VidPnSourceId, status);
		goto Cleanup;
	}

	status = sourceModeSetInterface->pfnCreateNewModeInfo(
		hSourceModeSet, &sourcemode
	);
	if (status != STATUS_SUCCESS) {
		pr_info("create source mode failed, status(0x%08x)\n", status);
		goto Cleanup;
	}
	
	sourcemode->Type = D3DKMDT_RMT_GRAPHICS;
	sourcemode->Format.Graphics.PrimSurfSize.cx = 1024;
	sourcemode->Format.Graphics.PrimSurfSize.cy = 768;
	sourcemode->Format.Graphics.VisibleRegionSize.cx = 1024;
	sourcemode->Format.Graphics.VisibleRegionSize.cy = 1768;
	sourcemode->Format.Graphics.Stride = 0x1000;
	sourcemode->Format.Graphics.PixelFormat = D3DDDIFMT_A8R8G8B8;
	sourcemode->Format.Graphics.ColorBasis = D3DKMDT_CB_SCRGB;
	sourcemode->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_DIRECT;

	status = sourceModeSetInterface->pfnAddMode(
		hSourceModeSet, 
		sourcemode
	);
	if (status != STATUS_SUCCESS) {
		pr_err("add source mode failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	sourcemode = NULL;

	status = VidPnInterface->pfnAssignSourceModeSet(
		hVidPn, VidPnSourceId, hSourceModeSet
	);
	if (status != STATUS_SUCCESS) {
		pr_err("assign source mode set failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	hSourceModeSet = NULL;
	sourceModeSetInterface = NULL;

Cleanup:
	if (sourcemode) {
		ASSERT(hSourceModeSet);
		ASSERT(sourceModeSetInterface != NULL);

		sourceModeSetInterface->pfnReleaseModeInfo(hSourceModeSet, sourcemode);
	}

	if (hSourceModeSet != NULL) {
		VidPnInterface->pfnReleaseSourceModeSet(hVidPn, hSourceModeSet);
	}

	return STATUS_SUCCESS;
}

NTSTATUS Filter_CreateTargetModeSet(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnTargetId
)
{
	NTSTATUS status;
	D3DKMDT_HVIDPNTARGETMODESET hTargetModeSet = NULL;
	const DXGK_VIDPNTARGETMODESET_INTERFACE* targetModeSetInterface;
	D3DKMDT_VIDPN_TARGET_MODE* targetmode = NULL;

	status = VidPnInterface->pfnCreateNewTargetModeSet(
		hVidPn, VidPnTargetId, &hTargetModeSet, &targetModeSetInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("create target mode set failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	status = targetModeSetInterface->pfnCreateNewModeInfo(
		hTargetModeSet, 
		&targetmode
	);
	if (status != STATUS_SUCCESS) {
		pr_err("create new target mode failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	targetmode->Id = VidPnTargetId;
	targetmode->VideoSignalInfo.VideoStandard = D3DKMDT_VSS_OTHER;
	targetmode->VideoSignalInfo.TotalSize.cx = 1024;
	targetmode->VideoSignalInfo.TotalSize.cy = 768;
	targetmode->VideoSignalInfo.ActiveSize.cx = 1024;
	targetmode->VideoSignalInfo.ActiveSize.cy = 768;

	targetmode->VideoSignalInfo.VSyncFreq.Numerator = 148500000;
	targetmode->VideoSignalInfo.VSyncFreq.Denominator = 2475000;
	targetmode->VideoSignalInfo.HSyncFreq.Numerator = 67500;
	targetmode->VideoSignalInfo.HSyncFreq.Denominator = 1;
	targetmode->VideoSignalInfo.PixelRate = 148500000;
	targetmode->VideoSignalInfo.ScanLineOrdering = D3DDDI_VSSLO_PROGRESSIVE;

	targetmode->Preference = D3DKMDT_MP_PREFERRED;

	status = targetModeSetInterface->pfnAddMode(
		hTargetModeSet, 
		targetmode
	);
	if (status != STATUS_SUCCESS) {
		pr_err("add target mode failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	targetmode = NULL;

	status = VidPnInterface->pfnAssignTargetModeSet(
		hVidPn, VidPnTargetId, hTargetModeSet
	);
	if (status != STATUS_SUCCESS) {
		pr_err("assign target mode set failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	hTargetModeSet = NULL;

Cleanup:

	if (targetmode != NULL) {
		ASSERT(hTargetModeSet);
		ASSERT(targetModeSetInterface);

		targetModeSetInterface->pfnReleaseModeInfo(hTargetModeSet, targetmode);
	}

	if (hTargetModeSet != NULL) {
		VidPnInterface->pfnReleaseTargetModeSet(hVidPn, hTargetModeSet);
	}

	return status;
}

