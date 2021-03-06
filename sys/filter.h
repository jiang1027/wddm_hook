#ifndef FILTER_INC
#define FILTER_INC

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>
#include <ntddvdeo.h>

#include <initguid.h>

#include <Dispmprt.h>
#include <d3dkmdt.h>

DRIVER_UNLOAD WddmHookUnload;

///定义VIDPN中子设备ID，
#define VIDPN_CHILD_UDID             0x667b0099


typedef __checkReturn NTSTATUS
DXGKRNL_DPIINITIALIZE(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath,
	DRIVER_INITIALIZATION_DATA* DriverInitData
);
typedef DXGKRNL_DPIINITIALIZE* PDXGKRNL_DPIINITIALIZE;


///////
struct vidpn_target_id
{
	LONG    num;
	D3DDDI_VIDEO_PRESENT_TARGET_ID ids[1];
};

struct vidpn_paths_t
{
	LONG               num_paths;
	struct vidpn_target_id*   target_paths[1];
};

struct vidpn_intf_t
{
	LIST_ENTRY                   list;
	///
    D3DKMDT_HVIDPN               hVidPn;
	DXGK_VIDPN_INTERFACE         vidpn_if, mod_vidpn_if;

	////
	D3DKMDT_HVIDPNTOPOLOGY              hTopology;
	DXGK_VIDPNTOPOLOGY_INTERFACE        topology_if, mod_topology_if;

	struct vidpn_paths_t*	paths; ////
};

struct _WDDM_DRIVER;

#define WDDM_ADAPTER_SIGNATURE 'adAW'

typedef struct _WDDM_ADAPTER
{
#if DBG
	ULONG Signature;
#endif

	LIST_ENTRY List;

	struct _WDDM_DRIVER* WddmDriver;

	PDEVICE_OBJECT PhysicalDeviceObject;
	PVOID MiniportDeviceContext;
	
	DXGKRNL_INTERFACE DxgkInterface;
	DXGKRNL_INTERFACE HookedDxgkInterface;

	ULONG NumberOfVideoPresentSources;
	ULONG NumberOfChildren;

	KTIMER timer;
	KDPC timerDpc;
	LARGE_INTEGER timerDueTime;

	BOOLEAN indicateVirtualDisplay;
	
	struct {
		BOOLEAN updatePending;
		LONG connectionChangeId;
		DXGK_CHILD_DESCRIPTOR Descriptor;
	} VirtualChild;

} WDDM_ADAPTER, *PWDDM_ADAPTER;


#define WDDM_DRIVER_SIGNATURE 'vrdW'

typedef struct _WDDM_DRIVER
{
#if DBG
	ULONG Signature;
#endif

	LIST_ENTRY List;

	DRIVER_INITIALIZATION_DATA DriverInitData;

	PDRIVER_OBJECT DriverObject;
	PDRIVER_ADD_DEVICE OrigAddDevice;

	KSPIN_LOCK Lock;
	LIST_ENTRY AdapterList;
} WDDM_DRIVER, *PWDDM_DRIVER;


typedef struct _WDDM_HOOK_GLOBAL
{
	PDRIVER_OBJECT          DriverObject;

	PDEVICE_OBJECT          ctrl_devobj;

	PFILE_OBJECT            dxgkrnl_fileobj;
	PDEVICE_OBJECT          dxgkrnl_pdoDevice;
	PDEVICE_OBJECT          dxgkrnl_nextDevice;

	PDXGKRNL_DPIINITIALIZE  dxgkrnl_dpiInit;

	///
	KSPIN_LOCK              spin_lock;
	KIRQL                   kirql;

	LIST_ENTRY              vidpn_if_head;
	LIST_ENTRY              topology_if_head;

	BOOLEAN fEnumVirtualChild;

	BOOLEAN fDumpSourceModeSet;
	BOOLEAN fDumpPinnedSourceMode;
	DWORD nMaxSourceModeSetDump;

	BOOLEAN fDumpTargetModeSet;
	BOOLEAN fDumpPinnedTargetMode;
	BOOLEAN fDumpPreferredTargetMode;
	DWORD nMaxTargetModeSetDump;

	DWORD nMaxVidPnDump;
	DWORD nVidPnDumped;

	BOOLEAN hooked;
	DRIVER_INITIALIZATION_DATA  orgDpiFunc;

	ULONG                       vidpn_source_count;
	ULONG                       vidpn_target_count;
	DXGKRNL_INTERFACE           DxgkInterface;

	BOOLEAN fCreateHookDevice;
	BOOLEAN fHookEnabled;
	BOOLEAN fModifyConstrainingVidPn;

	KSPIN_LOCK Lock;
	LIST_ENTRY HookDriverList;

} WDDM_HOOK_GLOBAL, *PWDDM_HOOK_GLOBAL;

extern WDDM_HOOK_GLOBAL Global;

PWDDM_DRIVER WddmDriverAlloc(PDRIVER_OBJECT DriverObject);
PWDDM_ADAPTER WddmAdapterAlloc(PWDDM_DRIVER WddmDriver);

PWDDM_DRIVER WddmHookFindDriver(PDRIVER_OBJECT DriverObject);

PWDDM_ADAPTER WddmHookFindFirstAdapter();
PWDDM_ADAPTER WddmHookFindAdapterFromPdo(PDEVICE_OBJECT PhysicalDeviceObject);
PWDDM_ADAPTER WddmHookFindAdapterFromContext(PVOID MiniportDeviceContext);
PWDDM_ADAPTER WddmHookFindAdapterFromDeviceHandle(HANDLE DeviceHandle);

#define WDDM_HOOK_FIND_FIRST_ADAPTER		0x00000001
#define WDDM_HOOK_FIND_MATCH_PDO			0x00000002
#define WDDM_HOOK_FIND_MATCH_DEVICE_CONTEXT 0x00000004
#define WDDM_HOOK_FIND_MATCH_DEVICE_HANDLE	0x00000008

typedef struct _WDDM_ADAPTER_FIND_DATA
{
	ULONG Flags;
	
	PDEVICE_OBJECT PhysicalDeviceObject;
	PVOID MiniportDeviceContext;
	HANDLE DeviceHandle;

} WDDM_ADAPTER_FIND_DATA, *PWDDM_ADAPTER_FIND_DATA;

PWDDM_ADAPTER WddmHookFindAdapter(PWDDM_ADAPTER_FIND_DATA FindData);

#define wf (&(Global))
#define wf_lock()   KeAcquireSpinLock(&wf->spin_lock, &wf->kirql);
#define wf_unlock() KeReleaseSpinLock(&wf->spin_lock, wf->kirql);

NTSTATUS create_wddm_filter_ctrl_device(PDRIVER_OBJECT drvObj);

NTSTATUS call_lower_driver(PIRP irp);

NTSTATUS DpiInitialize(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath,
	DRIVER_INITIALIZATION_DATA* DriverInitData);

NTSTATUS Win10DpiInitialize(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath,
	DRIVER_INITIALIZATION_DATA* DriverInitData);

NTSTATUS DxgkDdiEnumVidPnCofuncModality(CONST HANDLE  hAdapter,
	CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* CONST  pEnumCofuncModalityArg);

NTSTATUS DxgkDdiIsSupportedVidPn(
	IN_CONST_HANDLE hAdapter,
	INOUT_PDXGKARG_ISSUPPORTEDVIDPN pIsSupportedVidPn);

NTSTATUS DxgkDdiCommitVidPn(
	IN_CONST_HANDLE hAdapter,
	IN_CONST_PDXGKARG_COMMITVIDPN_CONST pCommitVidPn);

NTSTATUS DxgkDdiSetVidPnSourceVisibility(
	IN_CONST_HANDLE hAdapter,
	IN_CONST_PDXGKARG_SETVIDPNSOURCEVISIBILITY pSetVidPnSourceVisibility);

NTSTATUS APIENTRY DxgkDdiSetVidPnSourceAddress(
	const HANDLE                        hAdapter,
	const DXGKARG_SETVIDPNSOURCEADDRESS *pSetVidPnSourceAddress);


DXGKDDI_ADD_DEVICE Filter_DxgkDdiAddDevice;
DXGKDDI_START_DEVICE Filter_DxgkDdiStartDevice;
DXGKDDI_STOP_DEVICE Filter_DxgkDdiStopDevice;
DXGKDDI_REMOVE_DEVICE Filter_DxgkDdiRemoveDevice;
DXGKDDI_QUERY_CHILD_RELATIONS Filter_DxgkDdiQueryChildRelations;
DXGKDDI_QUERY_CHILD_STATUS Filter_DxgkDdiQueryChildStatus;
DXGKDDI_QUERY_DEVICE_DESCRIPTOR Filter_DxgkDdiQueryDeviceDescriptor;

DXGKDDI_ISSUPPORTEDVIDPN Filter_DxgkDdiIsSupportedVidPn;
DXGKDDI_ENUMVIDPNCOFUNCMODALITY Filter_DxgkDdiEnumVidPnCofuncModality;
DXGKDDI_SETVIDPNSOURCEADDRESS Filter_DxgkDdiSetVidPnSourceAddress;
DXGKDDI_SETVIDPNSOURCEVISIBILITY Filter_DxgkDdiSetVidPnSourceVisibility;
DXGKDDI_COMMITVIDPN Filter_DxgkDdiCommitVidPn;
DXGKDDI_RECOMMENDMONITORMODES Filter_DxgkDdiRecommendMonitorModes;
DXGKDDI_QUERYCONNECTIONCHANGE Filter_DxgkDdiQueryConnectionChange;

KDEFERRED_ROUTINE Win10WddmAdapterTimerRoutine;

#endif /* FILTER_INC */
/* end of file */
