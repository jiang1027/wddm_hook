#ifndef HELPERAPI_INC
#define HELPERAPI_INC

#include "filter.h"

char* wcs2strn(char* dest, WCHAR* src, size_t n);
WCHAR* str2wcsn(WCHAR* dest, char* src, size_t n);

#ifndef MAX
#define MAX(x, y)	((x) > (y) ? (x) : (y))
#endif 

#ifndef MIN
#define MIN(x, y)	((x) < (y) ? (x) : (y))
#endif 


#define WDDM_HOOK_MEMORY_TAG		'0khW'

#if DBG
#  define SIGNATURE_ASSIGN(p, s)	do { (p) = (s); } while (0)
#else 
#  define SIGNATURE_ASSIGN(p, s)	do {} while (0)
#endif

#if DBG
PVOID DebugAlloc(POOL_TYPE PoolType, SIZE_T nBytes, ULONG Tag);
void DebugFree(PVOID p);
#endif 

#if DBG
#  define _ALLOC(PoolType, nBytes, Tag)		DebugAlloc((PoolType), (nBytes), (Tag))
#  define _FREE(p)							do { DebugFree((p)); (p) = NULL; } while (0)
#else
#  define _ALLOC(PoolType, nBytes, Tag)		ExAllocatePoolWithTag((PoolTag), (nBytes), (Tag))
#  define _FREE(p)							do { ExFreePool((p)); } while (0)
#endif 

void Dump_DXGK_CHILD_DESCRIPTOR(PDXGK_CHILD_DESCRIPTOR ChildDescriptor, DWORD n);
void Dump_DXGK_CHILD_STATUS(DXGK_CHILD_STATUS * ChildStatus);
void Dump_DXGK_DEVICE_DESCRIPTOR(DXGK_DEVICE_DESCRIPTOR * Descriptor);
void Dump_DXGK_CHILD_DESCRIPTOR(PDXGK_CHILD_DESCRIPTOR ChildDescriptor, DWORD n);

void Dump_VidPn(PWDDM_ADAPTER WddmAdapter, D3DKMDT_HVIDPN hVidPn);
void Dump_DXGKARG_ENUMVIDPNCOFUNCMODALITY(PWDDM_ADAPTER wddmAdapter, const DXGKARG_ENUMVIDPNCOFUNCMODALITY* const EnumCofuncModality);
void Dump_DXGKARG_RECOMMENDMONITORMODES(PWDDM_ADAPTER wddmAdapter, const DXGKARG_RECOMMENDMONITORMODES* const RecommendMonitorModes);
void Dump_DXGKARG_COMMITVIDPN(PWDDM_ADAPTER WddmAdapter, const DXGKARG_COMMITVIDPN* CommitVidPn);


// copy from d3dkddi.h
//
typedef enum _DXGK_CONNECTION_STATUS
{
	ConnectionStatusUninitialized = 0,

	TargetStatusDisconnected = 4,
	TargetStatusConnected,
	TargetStatusJoined,

	MonitorStatusDisconnected = 8,
	MonitorStatusUnknown,
	MonitorStatusConnected,

	LinkConfigurationStarted = 12,
	LinkConfigurationFailed,
	LinkConfigurationSucceeded,
} DXGK_CONNECTION_STATUS, * PDXGK_CONNECTION_STATUS;

void Dump_DXGKARG_QUERYCONNECTIONCHANGE(PWDDM_ADAPTER WddmAdapter, const DXGKARG_QUERYCONNECTIONCHANGE * QueryConnectionChange);

#endif /* HELPERAPI_INC */
/* end of file */

