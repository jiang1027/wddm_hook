#include "filter.h"
#include "trace.h"

static NTSTATUS DxgkDdiAddDevice(
	IN_CONST_PDEVICE_OBJECT PhysicalDeviceObject,
	OUT PVOID *MiniportDeviceContext)
{
	pr_err("Hook: DxgkDdiAddDevice. \n");
	return wf->orgDpiFunc.DxgkDdiAddDevice(PhysicalDeviceObject, MiniportDeviceContext);
}
static NTSTATUS DxgkDdiRemoveDevice(IN PVOID MiniportDeviceContext)
{
	pr_err("Hook: DxgkDdiRemoveDevice\n");
	return wf->orgDpiFunc.DxgkDdiRemoveDevice(MiniportDeviceContext);
}

////HOOK DxgkCbQueryVidPnInterface, 挂钩DxgkCbQueryVidPnInterface相关的所有回调函数，用于欺骗原始驱动的Target Source 的 Path 查询.

//查询所有路径，并且按照SourceId归类
static struct vidpn_paths_t* enum_all_paths(IN_CONST_D3DKMDT_HVIDPNTOPOLOGY topology_handle,
	const DXGK_VIDPNTOPOLOGY_INTERFACE* topology_if )
{
	NTSTATUS status = STATUS_SUCCESS;
	SIZE_T num = 0;
	status = topology_if->pfnGetNumPaths(topology_handle, &num);
	if (num <= 0) {
		return NULL;
	}

	SIZE_T sz = sizeof(struct vidpn_paths_t) + sizeof(struct vidpn_target_id*) * wf->vidpn_source_count +
		wf->vidpn_source_count*( sizeof(struct vidpn_target_id) + num* sizeof(D3DDDI_VIDEO_PRESENT_TARGET_ID) );

	struct vidpn_paths_t* p = (struct vidpn_paths_t*)ExAllocatePoolWithTag(NonPagedPool, sz, 'FXSD');
	if (!p) {
		return NULL;
	}

	
	RtlZeroMemory(p, sz);
	////
	p->num_paths = (LONG)num;

	CHAR* ptr = (CHAR*)p + sizeof(struct vidpn_paths_t) + sizeof(struct vidpn_target_id*)*wf->vidpn_source_count;
	ULONG i;
	for (i = 0; i < wf->vidpn_source_count; ++i) {
		p->target_paths[i] = (struct vidpn_target_id*)( ptr + i* ( sizeof(struct vidpn_target_id) + num * sizeof(D3DDDI_VIDEO_PRESENT_TARGET_ID) ) );
	}
	////// 
	CONST D3DKMDT_VIDPN_PRESENT_PATH *curr_path_info;
	CONST D3DKMDT_VIDPN_PRESENT_PATH *next_path_info;
	status = topology_if->pfnAcquireFirstPathInfo(topology_handle, &curr_path_info);
	if (status == STATUS_GRAPHICS_DATASET_IS_EMPTY) {
		ExFreePool(p);
		return NULL;
	}
	else if (!NT_SUCCESS(status)) {
		ExFreePool(p);
		return NULL;
	}
	/////
	INT t_num = 0;
	do {
		///
		UINT sid = curr_path_info->VidPnSourceId;
		UINT did = curr_path_info->VidPnTargetId;
		if ( sid < (UINT)wf->vidpn_source_count) {
			///
			if (did != VIDPN_CHILD_UDID) {// skip my target path
				///
				LONG n = p->target_paths[sid]->num; 
				p->target_paths[sid]->num++;
				p->target_paths[sid]->ids[n] = did; 
				///
				t_num++;
			}
			///
		}

		///next 
		status = topology_if->pfnAcquireNextPathInfo(topology_handle, curr_path_info, &next_path_info);
		topology_if->pfnReleasePathInfo(topology_handle, curr_path_info);
		curr_path_info = next_path_info;

		if (status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET) { /// end
			curr_path_info = NULL;
			//		pr_err("pfnAcquireNextPathInfo no more data.\n");
			break;
		}
		else if (!NT_SUCCESS(status)) {
			curr_path_info = NULL;
			pr_err("pfnAcquireNextPathInfo err=0x%X\n", status);
			break;
		}
		////
	} while (TRUE);

	p->num_paths = t_num;
	///
	return p;
}

NTSTATUS pfnGetNumPaths(
	IN_CONST_D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology,
	OUT_PSIZE_T pNumPaths)
{
	NTSTATUS status = STATUS_INVALID_PARAMETER;
	DXGKDDI_VIDPNTOPOLOGY_GETNUMPATHS ptr_pfnGetNumPaths = NULL;
	wf_lock();
	for (PLIST_ENTRY entry = wf->topology_if_head.Flink; entry != &wf->topology_if_head; entry = entry->Flink) {
		struct vidpn_intf_t* intf = CONTAINING_RECORD(entry, struct vidpn_intf_t, list);
		if (intf->hTopology == hVidPnTopology) {
			ptr_pfnGetNumPaths = intf->topology_if.pfnGetNumPaths;
			if (intf->paths && pNumPaths) {
				*pNumPaths = intf->paths->num_paths;
				wf_unlock();
				pr_err("pfnGetNumPaths Cache called num=%d\n", *pNumPaths);
				return STATUS_SUCCESS;
			}
			break;
		}
	}
	wf_unlock();
	/////
	if (!ptr_pfnGetNumPaths) {
		return STATUS_INVALID_PARAMETER;
	}

	status = ptr_pfnGetNumPaths(hVidPnTopology, pNumPaths);
	pr_err("pfnGetNumPaths called num=%d\n", *pNumPaths );

	return status;
}
NTSTATUS pfnGetNumPathsFromSource(
	IN_CONST_D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology,
	IN_CONST_D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
	OUT_PSIZE_T pNumPathsFromSource)
{
	NTSTATUS status = STATUS_SUCCESS;
	DXGKDDI_VIDPNTOPOLOGY_GETNUMPATHSFROMSOURCE ptr_pfnGetNumPathsFromSource = NULL;
	wf_lock();
	for (PLIST_ENTRY entry = wf->topology_if_head.Flink; entry != &wf->topology_if_head; entry = entry->Flink) {
		struct vidpn_intf_t* intf = CONTAINING_RECORD(entry, struct vidpn_intf_t, list);
		if (intf->hTopology == hVidPnTopology) {
			ptr_pfnGetNumPathsFromSource = intf->topology_if.pfnGetNumPathsFromSource;
			if (intf->paths && pNumPathsFromSource && VidPnSourceId < wf->vidpn_source_count ) {
				*pNumPathsFromSource = intf->paths->target_paths[VidPnSourceId]->num;
				wf_unlock();
				pr_err("pfnGetNumPathsFromSource Cache called. num=%d\n", *pNumPathsFromSource);
				return STATUS_SUCCESS;
			}
			break;
		}
	}
	wf_unlock();
	////
	if (!ptr_pfnGetNumPathsFromSource) {
		return STATUS_INVALID_PARAMETER;
	}

	status = ptr_pfnGetNumPathsFromSource(hVidPnTopology, VidPnSourceId, pNumPathsFromSource);

	pr_err("pfnGetNumPathsFromSource called. num=%d\n", *pNumPathsFromSource);
	return status;
}
NTSTATUS pfnEnumPathTargetsFromSource(
	IN_CONST_D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology,
	IN_CONST_D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
	IN_CONST_D3DKMDT_VIDPN_PRESENT_PATH_INDEX VidPnPresentPathIndex,
	OUT_PD3DDDI_VIDEO_PRESENT_TARGET_ID pVidPnTargetId)
{
	NTSTATUS status = STATUS_SUCCESS;
	DXGKDDI_VIDPNTOPOLOGY_ENUMPATHTARGETSFROMSOURCE ptr_pfnEnumPathTargetsFromSource = NULL;
	wf_lock();
	for (PLIST_ENTRY entry = wf->topology_if_head.Flink; entry != &wf->topology_if_head; entry = entry->Flink) {
		struct vidpn_intf_t* intf = CONTAINING_RECORD(entry, struct vidpn_intf_t, list);
		if (intf->hTopology == hVidPnTopology) {
			ptr_pfnEnumPathTargetsFromSource = intf->topology_if.pfnEnumPathTargetsFromSource;
			if (intf->paths && VidPnSourceId < wf->vidpn_source_count && pVidPnTargetId ) {
				if (VidPnPresentPathIndex >= intf->paths->target_paths[VidPnSourceId]->num) {
					wf_unlock();
					pr_err("VidPnPresentPathIndex >= intf->paths->target_path_num[VidPnSourceId]\n");
					return STATUS_INVALID_PARAMETER;
				}
				*pVidPnTargetId = intf->paths->target_paths[VidPnSourceId]->ids[VidPnPresentPathIndex]; ////
				wf_unlock();
				pr_err("pfnEnumPathTargetsFromSource Cache called sourceId=%d, index=%d, targetid=%d, st=0x%X\n", VidPnSourceId, VidPnPresentPathIndex, *pVidPnTargetId, status);
				return STATUS_SUCCESS;
			}
			break;
		}
	}
	wf_unlock();
	/////
	if (!ptr_pfnEnumPathTargetsFromSource) {
		return STATUS_INVALID_PARAMETER;
	}

	status = ptr_pfnEnumPathTargetsFromSource(hVidPnTopology, VidPnSourceId, VidPnPresentPathIndex, pVidPnTargetId);

	pr_err("pfnEnumPathTargetsFromSource called sourceId=%d, index=%d, targetid=%d, st=0x%X\n", VidPnSourceId, VidPnPresentPathIndex, *pVidPnTargetId, status );

	return status;
}

static NTSTATUS skip_my_target_path(
	IN_CONST_D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology,
	IN_CONST_PD3DKMDT_VIDPN_PRESENT_PATH_CONST pVidPnPresentPathInfo,
	DEREF_OUT_CONST_PPD3DKMDT_VIDPN_PRESENT_PATH ppNextVidPnPresentPathInfo,
	DXGKDDI_VIDPNTOPOLOGY_ACQUIRENEXTPATHINFO ptr_pfnAcquireNextPathInfo, 
	DXGKDDI_VIDPNTOPOLOGY_RELEASEPATHINFO ptr_pfnReleasePathInfo)
{
	NTSTATUS status = STATUS_SUCCESS;
	CONST D3DKMDT_VIDPN_PRESENT_PATH* curr_path = pVidPnPresentPathInfo;

	do {
		if (curr_path->VidPnTargetId != VIDPN_CHILD_UDID) {//检测是否我们的target ID
			break;
		}
		/////skip my target id
		
		status = ptr_pfnAcquireNextPathInfo(hVidPnTopology, curr_path, ppNextVidPnPresentPathInfo );
		
		ptr_pfnReleasePathInfo(hVidPnTopology, curr_path); /// release pathinfo
		///
		if (status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET) {
			break;
		}
		else if (!NT_SUCCESS(status)) {
			break;
		}

		curr_path = *ppNextVidPnPresentPathInfo; ////
		/////

	} while (TRUE);
	///
	return status;
}
static NTSTATUS pfnAcquireFirstPathInfo(
	IN_CONST_D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology,
	DEREF_OUT_CONST_PPD3DKMDT_VIDPN_PRESENT_PATH ppFirstVidPnPresentPathInfo)
{
	NTSTATUS status = STATUS_SUCCESS;
	DXGKDDI_VIDPNTOPOLOGY_ACQUIREFIRSTPATHINFO ptr_pfnAcquireFirstPathInfo = NULL;
	DXGKDDI_VIDPNTOPOLOGY_ACQUIRENEXTPATHINFO ptr_pfnAcquireNextPathInfo = NULL;
	DXGKDDI_VIDPNTOPOLOGY_RELEASEPATHINFO ptr_pfnReleasePathInfo = NULL;
	wf_lock();
	for (PLIST_ENTRY entry = wf->topology_if_head.Flink; entry != &wf->topology_if_head; entry = entry->Flink) {
		struct vidpn_intf_t* intf = CONTAINING_RECORD(entry, struct vidpn_intf_t, list);
		if (intf->hTopology == hVidPnTopology) {
			ptr_pfnAcquireFirstPathInfo = intf->topology_if.pfnAcquireFirstPathInfo;
			ptr_pfnAcquireNextPathInfo = intf->topology_if.pfnAcquireNextPathInfo;
			ptr_pfnReleasePathInfo = intf->topology_if.pfnReleasePathInfo;
			break;
		}
	}
	wf_unlock();
	///
	if (!ptr_pfnAcquireFirstPathInfo) {
		pr_err("** pfnAcquireFirstPathInfo NULL.\n");
		return STATUS_INVALID_PARAMETER;
	}

	status = ptr_pfnAcquireFirstPathInfo(hVidPnTopology, ppFirstVidPnPresentPathInfo);

	if ( NT_SUCCESS(status) && status != STATUS_GRAPHICS_DATASET_IS_EMPTY ) {
		CONST D3DKMDT_VIDPN_PRESENT_PATH* curr_path = *ppFirstVidPnPresentPathInfo;

		status = skip_my_target_path(hVidPnTopology, curr_path, ppFirstVidPnPresentPathInfo, ptr_pfnAcquireNextPathInfo, ptr_pfnReleasePathInfo); ////
	}

//	pr_err("ppFirstVidPnPresentPathInfo called. st=0x%X\n", status );
	/////
	return status;
}
static NTSTATUS pfnAcquireNextPathInfo(
	IN_CONST_D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology,
	IN_CONST_PD3DKMDT_VIDPN_PRESENT_PATH_CONST pVidPnPresentPathInfo,
	DEREF_OUT_CONST_PPD3DKMDT_VIDPN_PRESENT_PATH ppNextVidPnPresentPathInfo)
{
	NTSTATUS status = STATUS_SUCCESS;
	DXGKDDI_VIDPNTOPOLOGY_ACQUIRENEXTPATHINFO ptr_pfnAcquireNextPathInfo = NULL;
	DXGKDDI_VIDPNTOPOLOGY_RELEASEPATHINFO ptr_pfnReleasePathInfo = NULL;
	wf_lock();
	for (PLIST_ENTRY entry = wf->topology_if_head.Flink; entry != &wf->topology_if_head; entry = entry->Flink) {
		struct vidpn_intf_t* intf = CONTAINING_RECORD(entry, struct vidpn_intf_t, list);
		if (intf->hTopology == hVidPnTopology) {
			ptr_pfnAcquireNextPathInfo = intf->topology_if.pfnAcquireNextPathInfo;
			ptr_pfnReleasePathInfo = intf->topology_if.pfnReleasePathInfo;
			break;
		}
	}
	wf_unlock();

	/////
	if (!ptr_pfnAcquireNextPathInfo) {
		pr_err("** pfnAcquireNextPathInfo NULL.\n");
		return STATUS_INVALID_PARAMETER;
	}

	status = ptr_pfnAcquireNextPathInfo(hVidPnTopology, pVidPnPresentPathInfo, ppNextVidPnPresentPathInfo );

	if (NT_SUCCESS(status) && status != STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET ) {
		CONST D3DKMDT_VIDPN_PRESENT_PATH* curr_path = *ppNextVidPnPresentPathInfo;

		status = skip_my_target_path(hVidPnTopology, curr_path, ppNextVidPnPresentPathInfo, ptr_pfnAcquireNextPathInfo, ptr_pfnReleasePathInfo); ////
	}

//	pr_err("pfnAcquireNextPathInfo called. st=0x%X\n", status );
	return status;
}

static NTSTATUS pfnGetTopology(
	IN_CONST_D3DKMDT_HVIDPN hVidPn,
	OUT_PD3DKMDT_HVIDPNTOPOLOGY phVidPnTopology,
	DEREF_OUT_CONST_PPDXGK_VIDPNTOPOLOGY_INTERFACE ppVidPnTopologyInterface)
{
	NTSTATUS status = STATUS_SUCCESS;
	DXGKDDI_VIDPN_GETTOPOLOGY ptr_pfnGetTopology = NULL;
	wf_lock();
	for (PLIST_ENTRY entry = wf->vidpn_if_head.Flink; entry != &wf->vidpn_if_head; entry = entry->Flink) {
		struct vidpn_intf_t* intf = CONTAINING_RECORD(entry, struct vidpn_intf_t, list);
		if (hVidPn == intf->hVidPn) {
			ptr_pfnGetTopology = intf->vidpn_if.pfnGetTopology;
			break;
		}
	}
	wf_unlock();

	if (!ptr_pfnGetTopology) {
		pr_err("pfnGetTopology==NULL.\n");
		return STATUS_INVALID_PARAMETER;
	}
	status = ptr_pfnGetTopology(hVidPn, phVidPnTopology, ppVidPnTopologyInterface);

//	pr_err("pfnGetTopology called.\n");
	if (NT_SUCCESS(status) && ppVidPnTopologyInterface && *ppVidPnTopologyInterface && phVidPnTopology ) {
		///重新计算不包含我们自己的target path的路径
		struct vidpn_paths_t* p =  enum_all_paths(*phVidPnTopology, *ppVidPnTopologyInterface); ///
		PLIST_ENTRY entry; 

		wf_lock(); 

		struct vidpn_intf_t* intf = NULL;
		for (entry = wf->topology_if_head.Flink; entry != &wf->topology_if_head; entry = entry->Flink) {
			struct vidpn_intf_t* it = CONTAINING_RECORD(entry, struct vidpn_intf_t, list);
			if (it->hTopology == *phVidPnTopology) {
				intf = it;
				if (intf->paths) { ExFreePool(intf->paths); intf->paths = NULL; }///
				break;
			}
		}
		if (!intf) {
			intf = (struct vidpn_intf_t*)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct vidpn_intf_t), 'FXSD');
			if (intf) {
				InsertTailList(&wf->topology_if_head, &intf->list);
				intf->hTopology = *phVidPnTopology; 
				intf->paths = NULL;
				/////
			}
		}
		if (intf) {
			intf->paths = p; 
			///
			intf->topology_if = **ppVidPnTopologyInterface;
			intf->mod_topology_if = intf->topology_if;
			*ppVidPnTopologyInterface = &intf->mod_topology_if; ///
			
		    ///替换函数
			intf->mod_topology_if.pfnGetNumPaths = pfnGetNumPaths;
			intf->mod_topology_if.pfnGetNumPathsFromSource = pfnGetNumPathsFromSource;
			intf->mod_topology_if.pfnEnumPathTargetsFromSource = pfnEnumPathTargetsFromSource;
			intf->mod_topology_if.pfnAcquireFirstPathInfo = pfnAcquireFirstPathInfo;
			intf->mod_topology_if.pfnAcquireNextPathInfo = pfnAcquireNextPathInfo;

			/////
		}
		////
		wf_unlock();
	}
	///
	return status;
}

static NTSTATUS DxgkCbQueryVidPnInterface(
	IN_CONST_D3DKMDT_HVIDPN hVidPn,
	IN_CONST_DXGK_VIDPN_INTERFACE_VERSION VidPnInterfaceVersion,
	DEREF_OUT_CONST_PPDXGK_VIDPN_INTERFACE ppVidPnInterface)
{
	NTSTATUS status = STATUS_SUCCESS;

	status = wf->DxgkInterface.DxgkCbQueryVidPnInterface(hVidPn, VidPnInterfaceVersion, ppVidPnInterface);
	/// 替换成自己的回调函数，用于屏蔽我们在Hook Driver里边的 Target .
	if (NT_SUCCESS(status) && ppVidPnInterface && *ppVidPnInterface ) {
		///
		PLIST_ENTRY entry; BOOLEAN find = FALSE; ///
		wf_lock();
		for (entry = wf->vidpn_if_head.Flink; entry != &wf->vidpn_if_head; entry = entry->Flink) {
			struct vidpn_intf_t* intf = CONTAINING_RECORD(entry, struct vidpn_intf_t, list);
			if (intf->hVidPn == hVidPn) {
				intf->vidpn_if = *(*ppVidPnInterface);

				intf->mod_vidpn_if = intf->vidpn_if;
				intf->mod_vidpn_if.pfnGetTopology = pfnGetTopology;
				////
				*ppVidPnInterface = &intf->mod_vidpn_if;
				find = TRUE;
				break;
			}
		}
		if (!find) {
			struct vidpn_intf_t* intf = (struct vidpn_intf_t*)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct vidpn_intf_t), 'Fxsd');
			if (intf) {
				intf->hVidPn = hVidPn;
				intf->vidpn_if = *(*ppVidPnInterface);

				intf->mod_vidpn_if = intf->vidpn_if;
				intf->mod_vidpn_if.pfnGetTopology = pfnGetTopology;
				///
				*ppVidPnInterface = &intf->mod_vidpn_if;

				InsertTailList(&wf->vidpn_if_head, &intf->list); ////
				////
			}
		}
		
		wf_unlock();
		////
	}
	////
	return status;
}

///////
static NTSTATUS DxgkDdiStartDevice(
	IN PVOID MiniportDeviceContext,
	IN PDXGK_START_INFO DxgkStartInfo,
	IN PDXGKRNL_INTERFACE DxgkInterface,
	OUT PULONG NumberOfVideoPresentSources,
	OUT PULONG NumberOfChildren)
{
	NTSTATUS status = STATUS_SUCCESS;
	
	////WDDM1.1 到 WDDM2.3 每个都会有不同定义，这里是WDK7下编译，因此只copy WDDM1.1的部分。
	wf->DxgkInterface = *DxgkInterface; /// save interface function,用于VIDPN设置

	///////替换原来的接口
	DxgkInterface->DxgkCbQueryVidPnInterface = DxgkCbQueryVidPnInterface; 
	
	//////
	status = wf->orgDpiFunc.DxgkDdiStartDevice(MiniportDeviceContext, DxgkStartInfo, DxgkInterface, NumberOfVideoPresentSources, NumberOfChildren);
	////
	DxgkInterface->DxgkCbQueryVidPnInterface = wf->DxgkInterface.DxgkCbQueryVidPnInterface; 
	
	///
	pr_err("Hook: DxgkDdiStartDevice status=0x%X.\n", status ); ///

	if (NT_SUCCESS(status)) {
		pr_err("org: DxgkDdiStartDevice, NumberOfVideoPresentSources=%d, NumberOfChildren=%d\n", *NumberOfVideoPresentSources, *NumberOfChildren);
		
		//// 分别增加 1，增加 source 和 target
		wf->vidpn_source_count = *NumberOfVideoPresentSources; // +1;
		wf->vidpn_target_count = *NumberOfChildren + 1;
		
		//////
		*NumberOfVideoPresentSources = wf->vidpn_source_count;
		*NumberOfChildren = wf->vidpn_target_count;

		////
	}
	////
	return status;
}
static NTSTATUS DxgkDdiStopDevice(IN PVOID MiniportDeviceContext)
{
	pr_err("Hook: DxgkDdiStopDevice.\n");
	return wf->orgDpiFunc.DxgkDdiStopDevice(MiniportDeviceContext);
}

static NTSTATUS DxgkDdiQueryChildRelations(IN PVOID pvMiniportDeviceContext, 
	IN OUT PDXGK_CHILD_DESCRIPTOR pChildRelations, 
	IN ULONG ChildRelationsSize)
{
	NTSTATUS status;

	status = wf->orgDpiFunc.DxgkDdiQueryChildRelations(pvMiniportDeviceContext, pChildRelations, ChildRelationsSize);
	pr_err("Hook: DxgkDdiQueryChildRelations status=0x%X\n", status);

	////
	if (NT_SUCCESS(status)) {
		////
		ULONG reqSize = sizeof(DXGK_CHILD_DESCRIPTOR)*wf->vidpn_target_count;
		if (reqSize > ChildRelationsSize) {
			return STATUS_BUFFER_TOO_SMALL;
		}

		/////
		pChildRelations[wf->vidpn_target_count - 1] = pChildRelations[0]; ///把第一个复制给我们的target

		pChildRelations[wf->vidpn_target_count - 1].ChildUid = VIDPN_CHILD_UDID; //定义我们的target vidpn的ID
		pChildRelations[wf->vidpn_target_count - 1].AcpiUid = VIDPN_CHILD_UDID;

		////
	}
	return status;
}
static NTSTATUS DxgkDdiQueryChildStatus(IN PVOID MiniportDeviceContext, IN PDXGK_CHILD_STATUS ChildStatus, IN BOOLEAN NonDestructiveOnly)
{
	pr_err("Hook: DxgkDdiQueryChildStatus Uid=0x%X\n", ChildStatus->ChildUid);

	if (ChildStatus->ChildUid == VIDPN_CHILD_UDID) {

		ChildStatus->HotPlug.Connected = TRUE; /// 
		///
		return STATUS_SUCCESS;
	}

	////
	return wf->orgDpiFunc.DxgkDdiQueryChildStatus(MiniportDeviceContext, ChildStatus, NonDestructiveOnly);
}
static NTSTATUS DxgkDdiQueryDeviceDescriptor(IN_CONST_PVOID MiniportDeviceContext, IN_ULONG ChildUid, INOUT_PDXGK_DEVICE_DESCRIPTOR DeviceDescriptor)
{
	pr_err("Hook: DxgkDdiQueryDeviceDescriptor Uid=0x%X\n", ChildUid);

	if (ChildUid == VIDPN_CHILD_UDID) {
		///
		return STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA;
	}
	////
	return wf->orgDpiFunc.DxgkDdiQueryDeviceDescriptor(MiniportDeviceContext, ChildUid, DeviceDescriptor);
}

DECLARE_CONST_UNICODE_STRING(vm_str, L"\\Driver\\vm3dmp_loader"); // Vmware 3D 
DECLARE_CONST_UNICODE_STRING(igfx_str, L"\\Driver\\igfx"); // Intel Graphics
DECLARE_CONST_UNICODE_STRING(nv_str, L"\\Driver\\nvlddmkm"); // Nvidia Graphics


NTSTATUS DpiInitialize(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath,
	DRIVER_INITIALIZATION_DATA* DriverInitData)
{

	pr_debug("-> DpiInitialize()\n");
	pr_debug("  DriverName: %S\n", DriverObject->DriverName.Buffer);

	if (!wf->hooked && 
		(RtlEqualUnicodeString(&vm_str, &DriverObject->DriverName, TRUE) || 
		 RtlEqualUnicodeString(&nv_str, &DriverObject->DriverName, TRUE)))
	{
		//这里只HOOK第一个显卡
		wf->hooked = TRUE;

		///
		//这里复制需要注意：
		// DRIVER_INITIALIZATION_DATA结构定义，WDDM1.1 到 WDDM2.3 每个都会有不同定义，这里是WDK7下编译，因此只copy WDDM1.1的部分。
		RtlCopyMemory(&wf->orgDpiFunc, DriverInitData, sizeof(DRIVER_INITIALIZATION_DATA));

		////replace some function
		DriverInitData->DxgkDdiAddDevice = DxgkDdiAddDevice;
		DriverInitData->DxgkDdiRemoveDevice = DxgkDdiRemoveDevice;
		DriverInitData->DxgkDdiStartDevice = DxgkDdiStartDevice;
		DriverInitData->DxgkDdiStopDevice = DxgkDdiStopDevice;
		DriverInitData->DxgkDdiQueryChildRelations = DxgkDdiQueryChildRelations;
		DriverInitData->DxgkDdiQueryChildStatus = DxgkDdiQueryChildStatus;
		DriverInitData->DxgkDdiQueryDeviceDescriptor = DxgkDdiQueryDeviceDescriptor;
		DriverInitData->DxgkDdiEnumVidPnCofuncModality = DxgkDdiEnumVidPnCofuncModality; ////
		DriverInitData->DxgkDdiIsSupportedVidPn = DxgkDdiIsSupportedVidPn;
		DriverInitData->DxgkDdiCommitVidPn = DxgkDdiCommitVidPn;
		DriverInitData->DxgkDdiSetVidPnSourceVisibility = DxgkDdiSetVidPnSourceVisibility;
		DriverInitData->DxgkDdiSetVidPnSourceAddress = DxgkDdiSetVidPnSourceAddress;
//		DriverInitData->DxgkDdiPresent = DxgkDdiPresent;

		/////
	}
	
	///替换了某些函数后，接着调用 dxgkrnl.sys 回调函数注册
	return wf->dxgkrnl_dpiInit(DriverObject, RegistryPath, DriverInitData);
}


void SetHookDriverInitData(DRIVER_INITIALIZATION_DATA* DriverInitData)
{
#define REPLACE_CALLBACK(name)	\
	if (DriverInitData->name != NULL) { DriverInitData->name = Filter_##name; }

	REPLACE_CALLBACK(DxgkDdiAddDevice);
	REPLACE_CALLBACK(DxgkDdiStartDevice);
	REPLACE_CALLBACK(DxgkDdiStopDevice);
	REPLACE_CALLBACK(DxgkDdiRemoveDevice);
	REPLACE_CALLBACK(DxgkDdiQueryChildRelations);
	REPLACE_CALLBACK(DxgkDdiQueryChildStatus);
	REPLACE_CALLBACK(DxgkDdiQueryDeviceDescriptor);
	REPLACE_CALLBACK(DxgkDdiIsSupportedVidPn);
	REPLACE_CALLBACK(DxgkDdiEnumVidPnCofuncModality);
	REPLACE_CALLBACK(DxgkDdiSetVidPnSourceAddress);
	REPLACE_CALLBACK(DxgkDdiSetVidPnSourceVisibility);
	REPLACE_CALLBACK(DxgkDdiCommitVidPn);
}

NTSTATUS Win10MonitorDpiInitialize(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath,
	DRIVER_INITIALIZATION_DATA* DriverInitData
)
{
	pr_debug("-> Win10MonitorDpiInitialize()\n");
	pr_debug("  DriverName: %S\n", DriverObject->DriverName.Buffer);

	if (!wf->hooked &&
		(RtlEqualUnicodeString(&vm_str, &DriverObject->DriverName, TRUE) ||
			RtlEqualUnicodeString(&nv_str, &DriverObject->DriverName, TRUE)))
	{
		wf->hooked = TRUE;

		pr_info("hooking DriverInitData version(%s)\n", 
			DxgkrnlVersionStr(DriverInitData->Version));

		RtlCopyMemory(&wf->orgDpiFunc, DriverInitData, sizeof(DRIVER_INITIALIZATION_DATA));

		SetHookDriverInitData(DriverInitData);
	}

	return wf->dxgkrnl_dpiInit(DriverObject, RegistryPath, DriverInitData);
}

