#ifndef DxgkCb_h__
#define DxgkCb_h__

#include "filter.h"

PDXGKRNL_INTERFACE Filter_SetupDxgkInterfaceHook(
	PWDDM_ADAPTER wddmAdapter, 
	PDXGKRNL_INTERFACE DxgkInterface
);

// hook functions for DXGKRNL_INTERFACE
//

NTSTATUS
APIENTRY Filter_DxgkCbEvalAcpiMethod(
	_In_ HANDLE DeviceHandle,
	_In_ ULONG DeviceUid,
	_In_reads_bytes_(AcpiInputSize) PACPI_EVAL_INPUT_BUFFER_COMPLEX AcpiInputBuffer,
	_In_range_(>= , sizeof(ACPI_EVAL_INPUT_BUFFER_COMPLEX)) ULONG AcpiInputSize,
	_Out_writes_bytes_(AcpiOutputSize) PACPI_EVAL_OUTPUT_BUFFER AcpiOutputBuffer,
	_In_range_(>= , sizeof(ACPI_EVAL_OUTPUT_BUFFER)) ULONG AcpiOutputSize
	);

NTSTATUS
APIENTRY Filter_DxgkCbGetDeviceInformation(
	_In_ HANDLE DeviceHandle,
	_Out_ PDXGK_DEVICE_INFO DeviceInfo
	);

NTSTATUS
APIENTRY Filter_DxgkCbIndicateChildStatus(
	_In_ HANDLE DeviceHandle,
	_In_ PDXGK_CHILD_STATUS ChildStatus
	);

NTSTATUS
APIENTRY Filter_DxgkCbMapMemory(
    _In_ HANDLE DeviceHandle,
    _In_ PHYSICAL_ADDRESS TranslatedAddress,
    _In_ ULONG Length,
    _In_ BOOLEAN InIoSpace,
    _In_ BOOLEAN MapToUserMode,
    _In_ MEMORY_CACHING_TYPE CacheType,
    _Outptr_ PVOID *VirtualAddress
    );

BOOLEAN
APIENTRY Filter_DxgkCbQueueDpc(
	_In_ HANDLE DeviceHandle
	);

NTSTATUS
APIENTRY Filter_DxgkCbQueryServices(
	_In_ HANDLE DeviceHandle,
	_In_ DXGK_SERVICES ServicesType,
	_Inout_ PINTERFACE Interface
	);

NTSTATUS
APIENTRY Filter_DxgkCbReadDeviceSpace(
	_In_ HANDLE DeviceHandle,
	_In_ ULONG DataType,
	_Out_writes_bytes_to_(Length, *BytesRead) PVOID Buffer,
	_In_ ULONG Offset,
	_In_ ULONG Length,
	_Out_ PULONG BytesRead
	);

NTSTATUS
APIENTRY Filter_DxgkCbSynchronizeExecution(
	_In_ HANDLE DeviceHandle,
	_In_ PKSYNCHRONIZE_ROUTINE SynchronizeRoutine,
	_In_ PVOID Context,
	_In_ ULONG MessageNumber,
	_Out_ PBOOLEAN ReturnValue
	);

NTSTATUS
APIENTRY Filter_DxgkCbUnmapMemory(
	_In_ HANDLE DeviceHandle,
	_In_ PVOID VirtualAddress
	);

NTSTATUS
APIENTRY Filter_DxgkCbWriteDeviceSpace(
	_In_ HANDLE DeviceHandle,
	_In_ ULONG DataType,
	_In_reads_bytes_(Length) PVOID Buffer,
	_In_ ULONG Offset,
	_In_ ULONG Length,
	_Out_ _Out_range_(<= , Length) PULONG BytesWritten
	);

NTSTATUS
APIENTRY Filter_DxgkCbIsDevicePresent(
	_In_ HANDLE DeviceHandle,
	_In_ PPCI_DEVICE_PRESENCE_PARAMETERS DevicePresenceParameters,
	_Out_ PBOOLEAN DevicePresent
	);


/*
VOID*
APIENTRY CALLBACK Filter_DxgkCbGetHandleData(
	IN_CONST_PDXGKARGCB_GETHANDLEDATA
);

D3DKMT_HANDLE
APIENTRY CALLBACK Filter_DxgkCbGetHandleParent(
	IN_D3DKMT_HANDLE hAllocation
);

D3DKMT_HANDLE
APIENTRY CALLBACK  Filter_DxgkCbEnumHandleChildren(
	IN_CONST_PDXGKARGCB_ENUMHANDLECHILDREN
);
*/

VOID 
APIENTRY CALLBACK Filter_DxgkCbNotifyInterrupt(
	IN_CONST_HANDLE hAdapter, IN_CONST_PDXGKARGCB_NOTIFY_INTERRUPT_DATA
	);

VOID
APIENTRY CALLBACK Filter_DxgkCbNotifyDpc(
	IN_CONST_HANDLE hAdapter
	);


NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbQueryMonitorInterface(
	IN_CONST_HANDLE                          hAdapter,
	IN_CONST_DXGK_MONITOR_INTERFACE_VERSION  MonitorInterfaceVersion,
	DEREF_OUT_CONST_PPDXGK_MONITOR_INTERFACE ppMonitorInterface
	);

/*
NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbGetCaptureAddress(
	INOUT_PDXGKARGCB_GETCAPTUREADDRESS
);


VOID
APIENTRY Filter_DxgkCbLogEtwEvent(
	_In_ CONST LPCGUID EventGuid,
	_In_ UCHAR Type,
	_In_ USHORT EventBufferSize,
	_In_reads_bytes_(EventBufferSize) PVOID EventBuffer
	);
*/

NTSTATUS
APIENTRY Filter_DxgkCbExcludeAdapterAccess(
	_In_ HANDLE DeviceHandle,
	_In_ ULONG Attributes,
	_In_ DXGKDDI_PROTECTED_CALLBACK DxgkProtectedCallback,
	_In_ PVOID ProtectedCallbackContext
	);


#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WIN8)

/*
NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbCreateContextAllocation(
	INOUT_PDXGKARGCB_CREATECONTEXTALLOCATION
	);
*/

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbDestroyContextAllocation(
	IN_CONST_HANDLE hAdapter,       // in: Adapter the allocation was created for.
	IN_CONST_HANDLE hAllocation     // in: DXG assigned handle for this allocation that was returned from DxgkCbCreateContextAllocation
									// as DXGKARGCB_CREATECONTEXTALLOCATION::hAllocation member.
	);

VOID
APIENTRY CALLBACK Filter_DxgkCbSetPowerComponentActive(
	IN_CONST_HANDLE hAdapter,
	UINT            ComponentIndex
	);

VOID
APIENTRY CALLBACK Filter_DxgkCbSetPowerComponentIdle(
	IN_CONST_HANDLE hAdapter,
	UINT            ComponentIndex
	);

NTSTATUS
APIENTRY Filter_DxgkCbAcquirePostDisplayOwnership(
	_In_ HANDLE DeviceHandle,
	_Out_ PDXGK_DISPLAY_INFORMATION DisplayInfo
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbPowerRuntimeControlRequest(
	IN_CONST_HANDLE hAdapter,
	IN LPCGUID      PowerControlCode,
	IN OPTIONAL     PVOID InBuffer,
	IN              SIZE_T InBufferSize,
	OUT OPTIONAL    PVOID OutBuffer,
	IN              SIZE_T OutBufferSize,
	OUT OPTIONAL    PSIZE_T BytesReturned
	);

VOID
APIENTRY CALLBACK Filter_DxgkCbSetPowerComponentLatency(
	IN_CONST_HANDLE hAdapter,
	UINT            ComponentIndex,
	ULONGLONG       Latency
	);

VOID
APIENTRY CALLBACK Filter_DxgkCbSetPowerComponentResidency(
	IN_CONST_HANDLE hAdapter,
	UINT            ComponentIndex,
	ULONGLONG       Residency
	);

VOID
APIENTRY CALLBACK Filter_DxgkCbCompleteFStateTransition(
	IN_CONST_HANDLE hAdapter,
	UINT            ComponentIndex
	);
;

#endif // DXGKDDI_INTERFACE_VERSION

#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM1_3)

VOID
APIENTRY CALLBACK Filter_DxgkCbCompletePStateTransition(
	IN_CONST_HANDLE hAdapter,
	IN UINT         ComponentIndex,
	IN UINT         CompletedPState
	);


#endif // DXGKDDI_INTERFACE_VERSION_WDDM1_3

#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_0)

D3DGPU_VIRTUAL_ADDRESS
APIENTRY CALLBACK Filter_DxgkCbMapContextAllocation(
	IN_CONST_HANDLE hAdapter,
	IN_CONST_PDXGKARGCB_MAPCONTEXTALLOCATION pArgs
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbUpdateContextAllocation(
	IN_CONST_HANDLE hAdapter,
	IN_CONST_PDXGKARGCB_UPDATECONTEXTALLOCATION pArgs
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbReserveGpuVirtualAddressRange(
	IN_CONST_HANDLE hAdapter,
	INOUT_PDXGKARGCB_RESERVEGPUVIRTUALADDRESSRANGE pArgs
	);

/*
VOID*
APIENTRY CALLBACK Filter_DxgkCbAcquireHandleData(
	IN_CONST_PDXGKARGCB_GETHANDLEDATA, 
	_Out_ PDXGKARG_RELEASE_HANDLE
);

VOID
APIENTRY CALLBACK Filter_DxgkCbReleaseHandleData(
	IN_CONST_DXGKARGCB_RELEASEHANDLEDATA
);
*/

VOID
APIENTRY CALLBACK Filter_DxgkCbHardwareContentProtectionTeardown(
	IN_CONST_HANDLE hAdapter,
	UINT Flags
	);


#endif // (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_0)

#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_1)

VOID
APIENTRY CALLBACK Filter_DxgkCbMultiPlaneOverlayDisabled(
	IN_CONST_HANDLE hAdapter,
	UINT VidPnSourceId
	);

VOID
APIENTRY CALLBACK Filter_DxgkCbMitigatedRangeUpdate(
	IN_CONST_HANDLE hAdapter,
	IN ULONG VirtualFunctionIndex
	);


#endif // (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_1)

#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_2)

/*
NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbInvalidateHwContext(
	IN_CONST_PDXGKARGCB_INVALIDATEHWCONTEXT
	);
*/

NTSTATUS
APIENTRY Filter_DxgkCbIndicateConnectorChange(
	IN_CONST_HANDLE     hAdapter
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbUnblockUEFIFrameBufferRanges(
	IN_CONST_HANDLE                   hAdapter,
	IN_CONST_PDXGK_SEGMENTMEMORYSTATE pSegmentMemoryState
);

NTSTATUS
APIENTRY Filter_DxgkCbAcquirePostDisplayOwnership2(
	_In_ HANDLE DeviceHandle,
	_Out_ PDXGK_DISPLAY_INFORMATION DisplayInfo,
	_Out_ PDXGK_DISPLAY_OWNERSHIP_FLAGS Flags
	);


#endif // (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_2)

#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_3)
/*
NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbSetProtectedSessionStatus(
	IN_CONST_PDXGKARGCB_PROTECTEDSESSIONSTATUS pProtectedSessionStatus
	);
*/
#endif // (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_3)

#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_4)
NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbAllocateContiguousMemory(
	IN_CONST_HANDLE                           hAdapter,
	INOUT_PDXGKARGCB_ALLOCATECONTIGUOUSMEMORY pAllocateContiguousMemory
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbFreeContiguousMemory(
	IN_CONST_HANDLE                          hAdapter,
	IN_CONST_PDXGKARGCB_FREECONTIGUOUSMEMORY pFreeContiguousMemory
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbAllocatePagesForMdl(
	IN_CONST_HANDLE                      hAdapter,
	INOUT_PDXGKARGCB_ALLOCATEPAGESFORMDL pAllocatePagesForMdl
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbFreePagesFromMdl(
	IN_CONST_HANDLE                       hAdapter,
	IN_CONST_PDXGKARGCB_FREEPAGESFROMMDL  pFreePagesFromMdl
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbPinFrameBufferForSave(
	IN_CONST_HANDLE                        hAdapter,
	INOUT_PDXGKARGCB_PINFRAMEBUFFERFORSAVE pPinFrameBufferForSave
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbUnpinFrameBufferForSave(
	IN_CONST_HANDLE                             hAdapter,
	IN_CONST_PDXGKARGCB_UNPINFRAMEBUFFERFORSAVE pUnpinFrameBufferForSave
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbMapFrameBufferPointer(
	IN_CONST_HANDLE                        hAdapter,
	INOUT_PDXGKARGCB_MAPFRAMEBUFFERPOINTER pMapFrameBufferPointer
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbUnmapFrameBufferPointer(
	IN_CONST_HANDLE                             hAdapter,
	IN_CONST_PDXGKARGCB_UNMAPFRAMEBUFFERPOINTER pUnmapFrameBufferPointer
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbMapMdlToIoMmu(
	IN_CONST_HANDLE                hAdapter,
	INOUT_PDXGKARGCB_MAPMDLTOIOMMU pMapMdlToIoMmu
	);

VOID
APIENTRY CALLBACK Filter_DxgkCbUnmapMdlFromIoMmu(
	IN_CONST_HANDLE                       hAdapter,
	IN_CONST_PDXGKARGCB_UNMAPMDLFROMIOMMU pUnmapMdlFromIoMmu
	);

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbReportDiagnostic(
	_In_ HANDLE                 DeviceHandle,
	IN_PDXGK_DIAGNOSTIC_HEADER  pDiagnostic
	);

#endif // (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_4)

#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_5)
/*
NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbSignalEvent(
	IN_CONST_PDXGKARGCB_SIGNALEVENT
);
*/
#endif // (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_5)

#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_6)

/*
NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbIsFeatureEnabled(
	INOUT_PDXGKARGCB_ISFEATUREENABLED
);
*/

NTSTATUS
APIENTRY CALLBACK Filter_DxgkCbSaveMemoryForHotUpdate(
	IN_CONST_HANDLE                             hAdapter,
	IN_CONST_PDXGKARGCB_SAVEMEMORYFORHOTUPDATE  pArgs
	);

#endif // (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WDDM2_6)


#endif // DxgkCb_h__
