#include "trace.h"

#include <ntddk.h>
#include <wdm.h>
#include <Dispmprt.h>
#include <ntstrsafe.h>

void TraceDump(const void* buf, unsigned int buflen, const char* prefix)
{
	static char HEX[] = "0123456789ABCDEF";
	unsigned char* p = (unsigned char*)buf;
	unsigned int i;
	char str[200] = { 0 };
	unsigned int len;

	Trace(LEVEL_INFO, "%s (length = %d):\n", prefix, buflen);

	for (i = 0; i < buflen; ++i) {
		if ((i % 16) == 0) {
			if (i > 0) {
				Trace(LEVEL_INFO, "%s\n", str);
			}
			RtlZeroMemory(str, sizeof(str));
		}

		str[(i % 16) * 3] = HEX[p[i] >> 4];
		str[(i % 16) * 3 + 1] = HEX[p[i] & 0x0F];
		str[(i % 16) * 3 + 2] = ' ';
	}

	if (strlen(str)) {
		Trace(LEVEL_INFO, "%s\n", str);
	}
}


const char* MajorFunctionStr[IRP_MJ_MAXIMUM_FUNCTION + 1] =
{
	/*IRP_MJ_CREATE*/				    "Create",
	/*IRP_MJ_CREATE_NAMED_PIPE*/	    "CreateNamedPipe",
	/*IRP_MJ_CLOSE*/				    "Close",
	/*IRP_MJ_READ*/					    "Read",
	/*IRP_MJ_WRITE*/				    "Write",
	/*IRP_MJ_QUERY_INFORMATION*/	    "QueryInformation",
	/*IRP_MJ_SET_INFORMATION*/		    "SetInformation",
	/*IRP_MJ_QUERY_EA*/				    "QueryEA",
	/*IRP_MJ_SET_EA*/				    "SetEA",
	/*IRP_MJ_FLUSH_BUFFERS*/		    "FlushBuffers",
	/*IRP_MJ_QUERY_VOLUME_INFORMATION*/	"QueryVolumeInformation",
	/*IRP_MJ_SET_VOLUME_INFORMATION*/	"SetVolumeInformation",
	/*IRP_MJ_DIRECTORY_CONTROL*/		"DirectoryControl",
	/*IRP_MJ_FILE_SYSTEM_CONTROL*/		"FileSystemControl",
	/*IRP_MJ_DEVICE_CONTROL*/			"DeviceControl",
	/*IRP_MJ_INTERNAL_DEVICE_CONTROL*/	"InternalDeviceControl",
	/*IRP_MJ_SHUTDOWN*/					"Shutdown",
	/*IRP_MJ_LOCK_CONTROL*/				"LockControl",
	/*IRP_MJ_CLEANUP*/					"Cleanup",
	/*IRP_MJ_CREATE_MAILSLOT*/			"CreateMailSlot",
	/*IRP_MJ_QUERY_SECURITY*/			"QuerySecurity",
	/*IRP_MJ_SET_SECURITY*/				"SetSecurity",
	/*IRP_MJ_POWER*/					"Power",
	/*IRP_MJ_SYSTEM_CONTROL*/			"SystemControl",
	/*IRP_MJ_DEVICE_CHANGE*/			"DeviceChange",
	/*IRP_MJ_QUERY_QUOTA*/				"QueryQuota",
	/*IRP_MJ_SET_QUOTA*/				"SetQuota",
	/*IRP_MJ_PNP*/						"PnP",
};

const char* DxgkrnlVersionStr(ULONG ver)
{
	static char buf[30];

	switch (ver)
	{
	case DXGKDDI_INTERFACE_VERSION_VISTA: return "Vista(0x1052)";
	case DXGKDDI_INTERFACE_VERSION_VISTA_SP1: return "VistaSP1(0x1053)";
	case DXGKDDI_INTERFACE_VERSION_WIN7: return "Win7(0x2005)";
	case DXGKDDI_INTERFACE_VERSION_WIN8: return "Win8(0x300E)";
	case DXGKDDI_INTERFACE_VERSION_WDDM1_3: return "WDDM1.3(0x4002)";
	case DXGKDDI_INTERFACE_VERSION_WDDM1_3_PATH_INDEPENDENT_ROTATION: return "WDDM1.3Patch(0x4003)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_0: return "WDDM2.0(0x5023)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_1: return "WDDM2.1(0x6003)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_1_5: return "WDDM2.1.5(0x6010)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_1_6: return "WDDM2.1.6(0x6011)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_2: return "WDDM2.2(0x700A)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_3: return "WDDM2.3(0x8001)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_4: return "WDDM2.4(0x9006)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_5: return "WDDM2.5(0xA00B)";
	case DXGKDDI_INTERFACE_VERSION_WDDM2_6: return "WDDM2.6(0xB004)";
	default:
		RtlStringCchPrintfA(buf, sizeof(buf), "Unknown(0x%04x)", ver);
		return buf;
	}
}