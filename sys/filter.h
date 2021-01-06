#ifndef FILTER_INC
#define FILTER_INC

#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>
#include <ntddvdeo.h>

#include <initguid.h>

#include <Dispmprt.h>
#include <d3dkmdt.h>


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

struct wddm_filter_t
{
	PDRIVER_OBJECT          driver_object;
	////
	PDEVICE_OBJECT          ctrl_devobj;
	
	////
	PFILE_OBJECT            dxgkrnl_fileobj;
	PDEVICE_OBJECT          dxgkrnl_pdoDevice;
	PDEVICE_OBJECT          dxgkrnl_nextDevice; ///

	PDXGKRNL_DPIINITIALIZE  dxgkrnl_dpiInit;

	///
	KSPIN_LOCK              spin_lock;
	KIRQL                   kirql;

	LIST_ENTRY              vidpn_if_head;
	LIST_ENTRY              topology_if_head;
	
	BOOLEAN hooked;
	DRIVER_INITIALIZATION_DATA  orgDpiFunc;

	ULONG                       vidpn_source_count;
	ULONG                       vidpn_target_count;
	DXGKRNL_INTERFACE           DxgkInterface;

};

extern struct wddm_filter_t WddmHookGlobal;
#define wf (&(WddmHookGlobal))
#define wf_lock()   KeAcquireSpinLock(&wf->spin_lock, &wf->kirql);
#define wf_unlock() KeReleaseSpinLock(&wf->spin_lock, wf->kirql);


NTSTATUS create_wddm_filter_ctrl_device(PDRIVER_OBJECT drvObj);

NTSTATUS call_lower_driver(PIRP irp);

NTSTATUS DpiInitialize(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath,
	DRIVER_INITIALIZATION_DATA* DriverInitData);

NTSTATUS Win10MonitorDpiInitialize(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath,
	DRIVER_INITIALIZATION_DATA* DriverInitData
);

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


DXGKDDI_ADD_DEVICE               Filter_DxgkDdiAddDevice;
DXGKDDI_START_DEVICE             Filter_DxgkDdiStartDevice;
DXGKDDI_STOP_DEVICE              Filter_DxgkDdiStopDevice;
DXGKDDI_REMOVE_DEVICE            Filter_DxgkDdiRemoveDevice;
DXGKDDI_QUERY_CHILD_RELATIONS    Filter_DxgkDdiQueryChildRelations;
DXGKDDI_QUERY_CHILD_STATUS       Filter_DxgkDdiQueryChildStatus;
DXGKDDI_QUERY_DEVICE_DESCRIPTOR  Filter_DxgkDdiQueryDeviceDescriptor;
DXGKDDI_ISSUPPORTEDVIDPN         Filter_DxgkDdiIsSupportedVidPn;
DXGKDDI_ENUMVIDPNCOFUNCMODALITY  Filter_DxgkDdiEnumVidPnCofuncModality;
DXGKDDI_SETVIDPNSOURCEADDRESS    Filter_DxgkDdiSetVidPnSourceAddress; 
DXGKDDI_SETVIDPNSOURCEVISIBILITY Filter_DxgkDdiSetVidPnSourceVisibility;
DXGKDDI_COMMITVIDPN              Filter_DxgkDdiCommitVidPn;


#endif /* FILTER_INC */
/* end of file */
