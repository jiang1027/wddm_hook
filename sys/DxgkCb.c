#include "filter.h"
#include "DxgkCb.h"
#include "trace.h"

static NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbQueryVidPnInterface(
	IN_CONST_D3DKMDT_HVIDPN                                 hVidPn,
	IN_CONST_DXGK_VIDPN_INTERFACE_VERSION                   VidPnInterfaceVersion,
	DEREF_OUT_CONST_PPDXGK_VIDPN_INTERFACE                  ppVidPnInterface
)
{
	NTSTATUS status;
	PWDDM_ADAPTER wddmAdapter;
	PDXGKRNL_INTERFACE dxgkInterface;

	pr_debug("-> Filter_DxgkCbQueryVidPnInterface()\n");

	wddmAdapter = WddmHookFindFirstAdapter();
	ASSERT(wddmAdapter);

	dxgkInterface = &wddmAdapter->DxgkInterface;
	ASSERT(dxgkInterface->DxgkCbQueryVidPnInterface);

	status = dxgkInterface->DxgkCbQueryVidPnInterface(
		hVidPn,
		VidPnInterfaceVersion,
		ppVidPnInterface
	);

	pr_debug("<- Filter_DxgkCbQueryVidPnInterface(), status(0x%08x)\n", status);
	return status;
}


PDXGKRNL_INTERFACE Filter_SetupDxgkInterfaceHook(
	PWDDM_ADAPTER wddmAdapter,
	PDXGKRNL_INTERFACE DxgkInterface
)
{
	RtlCopyMemory(
		&wddmAdapter->DxgkInterface,
		DxgkInterface,
		sizeof(*DxgkInterface)
	);

	RtlCopyMemory(
		&wddmAdapter->HookedDxgkInterface,
		DxgkInterface,
		sizeof(*DxgkInterface)
	);

	DxgkInterface = &wddmAdapter->HookedDxgkInterface;

	// DxgkInterface->DxgkCbQueryVidPnInterface = Filter_DxgkCbQueryVidPnInterface;

	return DxgkInterface;
}
