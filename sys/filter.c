#include "filter.h"
#include "ioctl.h"
#include "trace.h"
#include "dlpapi.h"
#include "helperapi.h"
#include "DxgkCb.h"

NTSTATUS Filter_HackConstainingVidPn(
	PWDDM_ADAPTER wddmAdapter,
	IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST pEnumCofuncModality
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

		if (Global.fChangeNumberOfChildren) {
			*NumberOfVideoPresentSources = 1;
			*NumberOfChildren = 1;
		}

		pr_debug("    NumberOfVideoPresentSources: %d\n", *NumberOfVideoPresentSources);
		pr_debug("    NumberOfChildren: %d\n", *NumberOfChildren);
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
	PDXGK_CHILD_DESCRIPTOR fullRelations = NULL;
	ULONG fullRelationsSize;

	wddmAdapter = WddmHookFindAdapterFromContext(MiniportDeviceContext);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiQueryChildRelations);

	pr_debug("-> Filter_DxgkDdiQueryChildRelations()\n");

	fullRelationsSize = (wddmAdapter->NumberOfChildren + 1) * sizeof(DXGK_CHILD_DESCRIPTOR);
	fullRelations = (PDXGK_CHILD_DESCRIPTOR)_ALLOC(NonPagedPool, fullRelationsSize, WDDM_HOOK_MEMORY_TAG);

	if (!fullRelations) {
		pr_err("allocate DXGK_CHILD_DESCRIPTOR failed\n");
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto End;
	}

	RtlZeroMemory(fullRelations, fullRelationsSize);

	status = DriverInitData->DxgkDdiQueryChildRelations(
		wddmAdapter->MiniportDeviceContext,
		fullRelations,
		fullRelationsSize
	);

	if (NT_SUCCESS(status)) {
		DWORD n = fullRelationsSize / sizeof(DXGK_CHILD_DESCRIPTOR);

		TraceIndent();
		Dump_DXGK_CHILD_DESCRIPTOR(fullRelations, n);
		TraceUnindent();

		n = ChildRelationsSize / sizeof(DXGK_CHILD_DESCRIPTOR);

		RtlCopyMemory(
			ChildRelations,
			fullRelations,
			(n - 1) * sizeof(DXGK_CHILD_DESCRIPTOR)
		);
	}

End:
	if (fullRelations) {
		_FREE(fullRelations);
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

	status = DriverInitData->DxgkDdiQueryDeviceDescriptor(
		wddmAdapter->MiniportDeviceContext,
		ChildUid,
		DeviceDescriptor
	);

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

	status = DriverInitData->DxgkDdiIsSupportedVidPn(
		wddmAdapter->MiniportDeviceContext,
		pIsSupportedVidPn
	);

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
	PWDDM_ADAPTER wddmAdapter;
	DRIVER_INITIALIZATION_DATA* DriverInitData;
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiEnumVidPnCofuncModality);

	pr_debug("-> Filter_DxgkDdiEnumVidPnCofuncModality()\n");
	TraceIndent();

	pr_info("original constraining VidPn\n");
	Dump_DXGKARG_ENUMVIDPNCOFUNCMODALITY(wddmAdapter, pEnumCofuncModality);
	
	status = DriverInitData->DxgkDdiEnumVidPnCofuncModality(
		wddmAdapter->MiniportDeviceContext,
		pEnumCofuncModality
	);

	if (NT_SUCCESS(status)) {
		if (Global.fModifyConstrainingVidPn) {
			status = Filter_HackConstainingVidPn(wddmAdapter, pEnumCofuncModality);
		}

		pr_info("result constraining VidPn\n");
		Dump_DXGKARG_ENUMVIDPNCOFUNCMODALITY(wddmAdapter, pEnumCofuncModality);
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
	NTSTATUS status;

	wddmAdapter = WddmHookFindAdapterFromContext(hAdapter);
	ASSERT(wddmAdapter && wddmAdapter->Signature == WDDM_ADAPTER_SIGNATURE);
	ASSERT(wddmAdapter->WddmDriver);

	DriverInitData = &wddmAdapter->WddmDriver->DriverInitData;
	ASSERT(DriverInitData->DxgkDdiQueryConnectionChange);

	pr_debug("-> Filter_DxgkDdiQueryconnectionchange()\n");

	status = DriverInitData->DxgkDdiQueryConnectionChange(hAdapter, pQueryConnectionChange);
	
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

NTSTATUS Filter_HackConstainingVidPn(
	PWDDM_ADAPTER wddmAdapter,
	IN_CONST_PDXGKARG_ENUMVIDPNCOFUNCMODALITY_CONST pEnumCofuncModality
)
{
	NTSTATUS status;
	const DXGK_VIDPN_INTERFACE* VidPnInterface;
	D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology = NULL;
	const DXGK_VIDPNTOPOLOGY_INTERFACE* topologyInterface = NULL;
	const D3DKMDT_VIDPN_PRESENT_PATH* path = NULL;
	const D3DKMDT_VIDPN_PRESENT_PATH* nextPath = NULL;

	ASSERT(wddmAdapter->DxgkInterface.DxgkCbQueryVidPnInterface);

	status = wddmAdapter->DxgkInterface.DxgkCbQueryVidPnInterface(
		pEnumCofuncModality->hConstrainingVidPn,
		DXGK_VIDPN_INTERFACE_VERSION_V1,
		&VidPnInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("DxgkCbQueryVidPnInterface() failed, status(0x%08x)\n", status);
		return status;
	}

	// get topology interface
	//
	status = VidPnInterface->pfnGetTopology(
		pEnumCofuncModality->hConstrainingVidPn,
		&hVidPnTopology,
		&topologyInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnGetTopology() failed, status(0x%08x)\n", status);
		return status;
	}

	// iterate through each path in topology
	//
	status = topologyInterface->pfnAcquireFirstPathInfo(hVidPnTopology, &path);
	while (status == STATUS_SUCCESS) {
		
		if (!((pEnumCofuncModality->EnumPivotType == D3DKMDT_EPT_VIDPNSOURCE) &&
			  (pEnumCofuncModality->EnumPivot.VidPnSourceId == path->VidPnSourceId)))
		{
			status = Filter_VidPnHasPinnedSource(
				pEnumCofuncModality->hConstrainingVidPn,
				VidPnInterface,
				path->VidPnSourceId
			);

			switch (status) {
			case STATUS_SUCCESS:
				break;

			case STATUS_NOT_FOUND:
				// no pinned source mode found, create new source mode set
				//
				status = Filter_CreateNewSourceModeSet(
					pEnumCofuncModality->hConstrainingVidPn,
					VidPnInterface,
					path->VidPnSourceId
				);
				break;

			default:
				break;
			}
		}
		
		// get next path and release the presented
		//
		status = topologyInterface->pfnAcquireNextPathInfo(
			hVidPnTopology,
			path,
			&nextPath
		);
		topologyInterface->pfnReleasePathInfo(hVidPnTopology, path);
	}

	return STATUS_SUCCESS;
}