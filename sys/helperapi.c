#include "helperapi.h"
#include "trace.h"

char* wcs2strn(char* dest, WCHAR* src, size_t n)
{
	ASSERT(n > 0);

	// reserve for tailing NULL
	n--;

	while (*src != L'\0' && n > 0) {
		*dest++ = (char)*src++;
		n--;
	}

	*dest++ = L'\0';

	return dest;
}

WCHAR* str2wcsn(WCHAR* dest, char* src, size_t n)
{
	ASSERT(n > 0);

	// reserve for tailing NULL
	n--;

	while (*src != '\0' && n > 0) {
		*dest++ = *src++;
		n--;
	}

	*dest++ = '\0';

	return dest;
}


#if DBG

PVOID DebugAlloc(POOL_TYPE PoolType, SIZE_T nBytes, ULONG Tag)
{
	PVOID p = ExAllocatePoolWithTagPriority(
		PoolType, nBytes, Tag, NormalPoolPriority
	);
	return p;
}

void DebugFree(PVOID p)
{
	ASSERT(p);
	ExFreePool(p);
}

#endif /* DBG */

const char* DXGK_CHILD_DEVICE_TYPE_Name(DXGK_CHILD_DEVICE_TYPE type)
{
	switch (type) {
	case TypeUninitialized: return "Uninitialized";
	case TypeVideoOutput  : return "VideoOutput";
	case TypeOther        : return "Other";
	default               : return "<unknown>";
	}
}

const char* D3DKMDT_VIDEO_OUTPUT_TECHNOLOGY_Name(D3DKMDT_VIDEO_OUTPUT_TECHNOLOGY v)
{
	switch (v) {
	case D3DKMDT_VOT_UNINITIALIZED       : return "Uninitialized";
	case D3DKMDT_VOT_OTHER               : return "Other";
	case D3DKMDT_VOT_HD15                : return "HD15";
	case D3DKMDT_VOT_SVIDEO              : return "SVideo";
	case D3DKMDT_VOT_COMPOSITE_VIDEO     : return "CompositeVideo";
	case D3DKMDT_VOT_COMPONENT_VIDEO     : return "ComponentVideo";
	case D3DKMDT_VOT_DVI                 : return "DVI";
	case D3DKMDT_VOT_HDMI                : return "HDMI";
	case D3DKMDT_VOT_LVDS                : return "LVDS";
	case D3DKMDT_VOT_D_JPN               : return "D_JPN";
	case D3DKMDT_VOT_SDI                 : return "SDI";
	case D3DKMDT_VOT_DISPLAYPORT_EXTERNAL: return "DisplayPortExternal";
	case D3DKMDT_VOT_DISPLAYPORT_EMBEDDED: return "DisplayPortEmbedded";
	case D3DKMDT_VOT_UDI_EXTERNAL        : return "UDIExterrnal";
	case D3DKMDT_VOT_UDI_EMBEDDED        : return "UDIEmbedded";
	case D3DKMDT_VOT_SDTVDONGLE          : return "SDTVDOngle";
	case D3DKMDT_VOT_INTERNAL            : return "Internal";
	default                              : return "<unknown>";
	}
}

const char* D3DKMDT_MONITOR_ORIENTATION_AWARENESS_Name(D3DKMDT_MONITOR_ORIENTATION_AWARENESS v)
{
	switch (v) {
	case D3DKMDT_MOA_UNINITIALIZED: return "Uninitialized";
	case D3DKMDT_MOA_NONE         : return "None";
	case D3DKMDT_MOA_POLLED       : return "Polled";
	case D3DKMDT_MOA_INTERRUPTIBLE: return "Interruptible";
	default                       : return "<unknown>";
	}
}

void Dump_DXGK_CHILD_DESCRIPTOR(
	PDXGK_CHILD_DESCRIPTOR ChildDescriptor,
	DWORD n
)
{
	DWORD i;
	for (i = 0; i < n; ++i, ++ChildDescriptor) {
		pr_info("DXGK_CHILD_DESCRIPTOR[%d] {\n", i);
		pr_info("    ChildDeviceType: %s\n", DXGK_CHILD_DEVICE_TYPE_Name(ChildDescriptor->ChildDeviceType));
		switch (ChildDescriptor->ChildDeviceType) {
		case TypeVideoOutput:
			pr_info(
				"    ChildCapabilities {\n"
				"        InterfaceTechnology(%s)\n"
				"        MonitorOrientationAwareness(%s)\n"
			    "        SupportsSdtvModes(%s)\n"
				"    }\n",
				D3DKMDT_VIDEO_OUTPUT_TECHNOLOGY_Name(ChildDescriptor->ChildCapabilities.Type.VideoOutput.InterfaceTechnology),
				D3DKMDT_MONITOR_ORIENTATION_AWARENESS_Name(ChildDescriptor->ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness),
				ChildDescriptor->ChildCapabilities.Type.VideoOutput.SupportsSdtvModes ? "True" : "False"
				);
			break;
		case TypeOther:
			pr_info("    ChildCapabilities { MustBeZero(%d) }\n",
				ChildDescriptor->ChildCapabilities.Type.Other.MustBeZero);
			break;
		default:
			break;
		}
		pr_info("    AcpiUid: %d\n", ChildDescriptor->AcpiUid);
		pr_info("    ChildUid: %d\n", ChildDescriptor->ChildUid);
		pr_info("}\n");
	}
}

const char* DXGK_CHILD_STATUS_TYPE_Name(DXGK_CHILD_STATUS_TYPE type)
{
	switch (type) {
	case StatusUninitialized: return "Uninitialized";
	case StatusConnection   : return "Connection";
	case StatusRotation     : return "Rotation";
	default                 : return "<unknown>";
	}
}

const char* D3DKMDT_VIDPN_PRESENT_PATH_SCALING_Name(D3DKMDT_VIDPN_PRESENT_PATH_SCALING scaling)
{
	switch (scaling) {
	case D3DKMDT_VPPS_UNINITIALIZED         : return "Uninitialized";
	case D3DKMDT_VPPS_IDENTITY              : return "Identity";
	case D3DKMDT_VPPS_CENTERED              : return "Centered";
	case D3DKMDT_VPPS_STRETCHED             : return "Stretched";
	case D3DKMDT_VPPS_ASPECTRATIOCENTEREDMAX: return "AspectRatioCenteredMax";
	case D3DKMDT_VPPS_CUSTOM                : return "Custom";
	case D3DKMDT_VPPS_RESERVED1             : return "Reserved1";
	case D3DKMDT_VPPS_UNPINNED              : return "Unpined";
	case D3DKMDT_VPPS_NOTSPECIFIED          : return "NotSpecified";
	default                                 : return "<unknown>";
	}
}

const char* D3DKMDT_VIDPN_PRESENT_PATH_SCALING_SUPPORT_Name(D3DKMDT_VIDPN_PRESENT_PATH_SCALING_SUPPORT ScalingSupport)
{
	static char name[80];
	name[0] = '\0';

	if (ScalingSupport.Identity) {
		strncat(name, "Identity ", sizeof(name));
	}
	if (ScalingSupport.Centered) {
		strncat(name, "Centered ", sizeof(name));
	}
	if (ScalingSupport.Stretched) {
		strncat(name, "Stretched ", sizeof(name));
	}
	if (ScalingSupport.AspectRatioCenteredMax) {
		strncat(name, "AspectRatioCenterMax ", sizeof(name));
	}
	if (ScalingSupport.Custom) {
		strncat(name, "Custom ", sizeof(name));
	}

	return name;
}

const char* D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_Name(D3DKMDT_VIDPN_PRESENT_PATH_ROTATION Rotation)
{
	switch (Rotation) {
	case D3DKMDT_VPPR_UNINITIALIZED: return "Uninitialized";
	case D3DKMDT_VPPR_IDENTITY     : return "Identity";
	case D3DKMDT_VPPR_ROTATE90     : return "Rotate90";
	case D3DKMDT_VPPR_ROTATE180    : return "Rotate180";
	case D3DKMDT_VPPR_ROTATE270    : return "Rotate270";
	case D3DKMDT_VPPR_UNPINNED     : return "Unpined";
	case D3DKMDT_VPPR_NOTSPECIFIED : return "NotSpecified";
	default                        : return "<unknown>";
	}
}


const char* D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_SUPPORT_Name(D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_SUPPORT RotationSupport)
{
	static char name[50];
	name[0] = '\0';
	if (RotationSupport.Identity) {
		strncat(name, "Identity ", sizeof(name));
	}
	if (RotationSupport.Rotate90) {
		strncat(name, "Rotate90 ", sizeof(name));
	}
	if (RotationSupport.Rotate180) {
		strncat(name, "Rotate180 ", sizeof(name));
	}
	if (RotationSupport.Rotate270) {
		strncat(name, "Rotate270 ", sizeof(name));
	}
	return name;
}

const char* D3DKMDT_COLOR_BASIS_Name(D3DKMDT_COLOR_BASIS ColorBasis)
{
	switch (ColorBasis) {
	case D3DKMDT_CB_UNINITIALIZED: return "Uninitialized";
	case D3DKMDT_CB_INTENSITY: return "Intensity";
	case D3DKMDT_CB_SRGB: return "SRGB";
	case D3DKMDT_CB_SCRGB: return "SCRGB";
	case D3DKMDT_CB_YCBCR: return "YCBCR";
	case D3DKMDT_CB_YPBPR: return "YPBPR";
	default: return "<unknown>";
	}
}

const char* D3DKMDT_PIXEL_VALUE_ACCESS_MODE_Name(D3DKMDT_PIXEL_VALUE_ACCESS_MODE mode)
{
	switch (mode) {
	case D3DKMDT_PVAM_UNINITIALIZED: return "Uninitialized";
	case D3DKMDT_PVAM_DIRECT: return "Direct";
	case D3DKMDT_PVAM_PRESETPALETTE: return "PresentPalette";
	case D3DKMDT_PVAM_SETTABLEPALETTE: return "SettablePalette";
	default: return "<unknown>";
	}
}

void Dump_DXGK_CHILD_STATUS(DXGK_CHILD_STATUS* ChildStatus)
{
	pr_info("DXGK_CHILD_STATUS {\n");
	pr_info("    Type: %s\n", DXGK_CHILD_STATUS_TYPE_Name(ChildStatus->Type));
	pr_info("    ChildUid: %d\n", ChildStatus->ChildUid);
	switch (ChildStatus->Type) {
	case StatusConnection:
		pr_info("    Connected: %d\n", ChildStatus->HotPlug.Connected);
		break;
	case StatusRotation:
		pr_info("    Angle: %d\n", ChildStatus->Rotation.Angle);
		break;
	default:
		break;
	}
	pr_info("}\n");
}

void Dump_DXGK_DEVICE_DESCRIPTOR(DXGK_DEVICE_DESCRIPTOR* Descriptor)
{
	pr_info("DXGK_DEVICE_DESCRIPTOR {\n");
	pr_info("    DescriptorOffset: %d\n", Descriptor->DescriptorOffset);
	pr_info("    DescriptorLength: %d\n", Descriptor->DescriptorLength);
	pr_info("    DescriptorBuffer: %p\n", Descriptor->DescriptorBuffer);
	pr_info("}\n");

	TraceDump(Descriptor->DescriptorBuffer, Descriptor->DescriptorLength, "EDID");
}

const char* D3DKMDT_VIDPN_SOURCE_MODE_TYPE_Name(D3DKMDT_VIDPN_SOURCE_MODE_TYPE type)
{
	switch (type) {
	case D3DKMDT_RMT_UNINITIALIZED: return "Uninitialized";
	case D3DKMDT_RMT_GRAPHICS     : return "Graphics";
	case D3DKMDT_RMT_TEXT         : return "Text";
	default                       : return "<unknown>";
	}
}

const char* D3DDDIFORMAT_Name(D3DDDIFORMAT format)
{
	switch (format)
	{
	case D3DDDIFMT_UNKNOWN                : return "Unknown";
	case D3DDDIFMT_R8G8B8                 : return "R8g8b8";
	case D3DDDIFMT_A8R8G8B8               : return "A8r8g8b8";
	case D3DDDIFMT_X8R8G8B8               : return "X8r8g8b8";
	case D3DDDIFMT_R5G6B5                 : return "R5g6b5";
	case D3DDDIFMT_X1R5G5B5               : return "X1r5g5b5";
	case D3DDDIFMT_A1R5G5B5               : return "A1r5g5b5";
	case D3DDDIFMT_A4R4G4B4               : return "A4r4g4b4";
	case D3DDDIFMT_R3G3B2                 : return "R3g3b2";
	case D3DDDIFMT_A8                     : return "A8";
	case D3DDDIFMT_A8R3G3B2               : return "A8r3g3b2";
	case D3DDDIFMT_X4R4G4B4               : return "X4r4g4b4";
	case D3DDDIFMT_A2B10G10R10            : return "A2b10g10r10";
	case D3DDDIFMT_A8B8G8R8               : return "A8b8g8r8";
	case D3DDDIFMT_X8B8G8R8               : return "X8b8g8r8";
	case D3DDDIFMT_G16R16                 : return "G16r16";
	case D3DDDIFMT_A2R10G10B10            : return "A2r10g10b10";
	case D3DDDIFMT_A16B16G16R16           : return "A16b16g16r16";
	case D3DDDIFMT_A8P8                   : return "A8p8";
	case D3DDDIFMT_P8                     : return "P8";
	case D3DDDIFMT_L8                     : return "L8";
	case D3DDDIFMT_A8L8                   : return "A8l8";
	case D3DDDIFMT_A4L4                   : return "A4l4";
	case D3DDDIFMT_V8U8                   : return "V8u8";
	case D3DDDIFMT_L6V5U5                 : return "L6v5u5";
	case D3DDDIFMT_X8L8V8U8               : return "X8l8v8u8";
	case D3DDDIFMT_Q8W8V8U8               : return "Q8w8v8u8";
	case D3DDDIFMT_V16U16                 : return "V16u16";
	case D3DDDIFMT_W11V11U10              : return "W11v11u10";
	case D3DDDIFMT_A2W10V10U10            : return "A2w10v10u10";
	case D3DDDIFMT_UYVY                   : return "UYVY";
	case D3DDDIFMT_R8G8_B8G8              : return "R8g8_B8G8";
	case D3DDDIFMT_YUY2                   : return "Yuy2";
	case D3DDDIFMT_G8R8_G8B8              : return "G8r8_G8B8";
	case D3DDDIFMT_DXT1                   : return "Dxt1";
	case D3DDDIFMT_DXT2                   : return "Dxt2";
	case D3DDDIFMT_DXT3                   : return "Dxt3";
	case D3DDDIFMT_DXT4                   : return "Dxt4";
	case D3DDDIFMT_DXT5                   : return "Dxt5";
	case D3DDDIFMT_D16_LOCKABLE           : return "D16_LOCKABLE";
	case D3DDDIFMT_D32                    : return "D32";
	case D3DDDIFMT_D15S1                  : return "D15s1";
	case D3DDDIFMT_D24S8                  : return "D24s8";
	case D3DDDIFMT_D24X8                  : return "D24x8";
	case D3DDDIFMT_D24X4S4                : return "D24x4s4";
	case D3DDDIFMT_D16                    : return "D16";
	case D3DDDIFMT_D32F_LOCKABLE          : return "D32f_LOCKABLE";
	case D3DDDIFMT_D24FS8                 : return "D24fs8";
	case D3DDDIFMT_D32_LOCKABLE           : return "D32_LOCKABLE";
	case D3DDDIFMT_S8_LOCKABLE            : return "S8_LOCKABLE";
	case D3DDDIFMT_S1D15                  : return "S1d15";
	case D3DDDIFMT_S8D24                  : return "S8d24";
	case D3DDDIFMT_X8D24                  : return "X8d24";
	case D3DDDIFMT_X4S4D24                : return "X4s4d24";
	case D3DDDIFMT_L16                    : return "L16";
	case D3DDDIFMT_VERTEXDATA             : return "VertexData";
	case D3DDDIFMT_INDEX16                : return "Index16";
	case D3DDDIFMT_INDEX32                : return "Index32";
	case D3DDDIFMT_Q16W16V16U16           : return "Q16w16v16u16";
	case D3DDDIFMT_MULTI2_ARGB8           : return "Multi2_ARGB8";
	case D3DDDIFMT_R16F                   : return "R16f";
	case D3DDDIFMT_G16R16F                : return "G16r16f";
	case D3DDDIFMT_A16B16G16R16F          : return "A16b16g16r16f";
	case D3DDDIFMT_R32F                   : return "R32f";
	case D3DDDIFMT_G32R32F                : return "G32r32f";
	case D3DDDIFMT_A32B32G32R32F          : return "A32b32g32r32f";
	case D3DDDIFMT_CxV8U8                 : return "Cxv8u8";
	case D3DDDIFMT_A1                     : return "A1";
	case D3DDDIFMT_A2B10G10R10_XR_BIAS    : return "A2b10g10r10_XR_BIAS";
	case D3DDDIFMT_PICTUREPARAMSDATA      : return "PictureParamsData";
	case D3DDDIFMT_MACROBLOCKDATA         : return "MacroBlockData";
	case D3DDDIFMT_RESIDUALDIFFERENCEDATA : return "ResidualDifferenceData";
	case D3DDDIFMT_DEBLOCKINGDATA         : return "DeblockingData";
	case D3DDDIFMT_INVERSEQUANTIZATIONDATA: return "InversequantizationData";
	case D3DDDIFMT_SLICECONTROLDATA       : return "SliceControlData";
	case D3DDDIFMT_BITSTREAMDATA          : return "BitstreamData";
	case D3DDDIFMT_MOTIONVECTORBUFFER     : return "MotionVectorBuffer";
	case D3DDDIFMT_FILMGRAINBUFFER        : return "FilmGrainBuffer";
	case D3DDDIFMT_DXVA_RESERVED9         : return "Dxva_RESERVED9";
	case D3DDDIFMT_DXVA_RESERVED10        : return "Dxva_RESERVED10";
	case D3DDDIFMT_DXVA_RESERVED11        : return "Dxva_RESERVED11";
	case D3DDDIFMT_DXVA_RESERVED12        : return "Dxva_RESERVED12";
	case D3DDDIFMT_DXVA_RESERVED13        : return "Dxva_RESERVED13";
	case D3DDDIFMT_DXVA_RESERVED14        : return "Dxva_RESERVED14";
	case D3DDDIFMT_DXVA_RESERVED15        : return "Dxva_RESERVED15";
	case D3DDDIFMT_DXVA_RESERVED16        : return "Dxva_RESERVED16";
	case D3DDDIFMT_DXVA_RESERVED17        : return "Dxva_RESERVED17";
	case D3DDDIFMT_DXVA_RESERVED18        : return "Dxva_RESERVED18";
	case D3DDDIFMT_DXVA_RESERVED19        : return "Dxva_RESERVED19";
	case D3DDDIFMT_DXVA_RESERVED20        : return "Dxva_RESERVED20";
	case D3DDDIFMT_DXVA_RESERVED21        : return "Dxva_RESERVED21";
	case D3DDDIFMT_DXVA_RESERVED22        : return "Dxva_RESERVED22";
	case D3DDDIFMT_DXVA_RESERVED23        : return "Dxva_RESERVED23";
	case D3DDDIFMT_DXVA_RESERVED24        : return "Dxva_RESERVED24";
	case D3DDDIFMT_DXVA_RESERVED25        : return "Dxva_RESERVED25";
	case D3DDDIFMT_DXVA_RESERVED26        : return "Dxva_RESERVED26";
	case D3DDDIFMT_DXVA_RESERVED27        : return "Dxva_RESERVED27";
	case D3DDDIFMT_DXVA_RESERVED28        : return "Dxva_RESERVED28";
	case D3DDDIFMT_DXVA_RESERVED29        : return "Dxva_RESERVED29";
	case D3DDDIFMT_DXVA_RESERVED30        : return "Dxva_RESERVED30";
	case D3DDDIFMT_DXVA_RESERVED31        : return "Dxva_RESERVED31";
	case D3DDDIFMT_BINARYBUFFER           : return "BinaryBuffer";
	case D3DDDIFMT_FORCE_UINT             : return "Force_UINT";
	default                               : return "<unknown>";
	}
}

void Dump_D3DKMDT_VIDPN_SOURCE_MODE(
	const D3DKMDT_VIDPN_SOURCE_MODE* SourceMode,
	DWORD index,
	const char* hints
)
{

	pr_info("SourceMode [%d] %s {\n", index, hints ? hints : "");
	pr_info("    Type: %s\n", D3DKMDT_VIDPN_SOURCE_MODE_TYPE_Name(SourceMode->Type));
	switch (SourceMode->Type) {
	case D3DKMDT_RMT_GRAPHICS:
		pr_info(
			"    Graphics {\n"
			"        PrimSurfSize: %d-%d\n"
			"        VisibleRegionSize: %d-%d\n"
			"        Stride: %d\n"
			"        PixelFormat: %s\n"
			"        ColorBasis: %s\n"
			"        PixelValueAccessMode: %s\n"
			"    }\n",
			SourceMode->Format.Graphics.PrimSurfSize.cx,
			SourceMode->Format.Graphics.PrimSurfSize.cy,
			SourceMode->Format.Graphics.VisibleRegionSize.cx,
			SourceMode->Format.Graphics.VisibleRegionSize.cy,
			SourceMode->Format.Graphics.Stride,
			D3DDDIFORMAT_Name(SourceMode->Format.Graphics.PixelFormat),
			D3DKMDT_COLOR_BASIS_Name(SourceMode->Format.Graphics.ColorBasis),
			D3DKMDT_PIXEL_VALUE_ACCESS_MODE_Name(SourceMode->Format.Graphics.PixelValueAccessMode)
		);
		break;

	case D3DKMDT_RMT_TEXT:
		break;

	default:
		break;
	}
	pr_info("}\n");
}

void Dump_SourceModeSet(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId
)
{
	D3DKMDT_HVIDPNSOURCEMODESET hSourceModeSet;
	const DXGK_VIDPNSOURCEMODESET_INTERFACE* sourceModeSetInterface;
	D3DKMDT_VIDPN_SOURCE_MODE* sourceMode = NULL;
	D3DKMDT_VIDPN_SOURCE_MODE* nextSourceMode;
	D3DKMDT_VIDPN_SOURCE_MODE* pinnedMode = NULL;
	NTSTATUS status;
	DWORD index = 0;
	SIZE_T num;

	status = VidPnInterface->pfnAcquireSourceModeSet(
		hVidPn,
		VidPnSourceId,
		&hSourceModeSet,
		&sourceModeSetInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnAcquireSourceModeSet() failed, status(0x%08x)\n", status);
		return;
	}

	status = sourceModeSetInterface->pfnGetNumModes(hSourceModeSet, &num);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnGetNumModes() failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	pr_info("NumSourceModes: %d\n", num);

	if (num == 0)
		goto Cleanup;

	status = sourceModeSetInterface->pfnAcquireFirstModeInfo(
		hSourceModeSet, &sourceMode
	);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnAcquireFirstModeInfo() failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	if (Global.fDumpPinnedSourceMode) {
		status = sourceModeSetInterface->pfnAcquirePinnedModeInfo(
			hSourceModeSet, &pinnedMode
		);
		if (status == STATUS_SUCCESS) {
			ASSERT(pinnedMode);

			Dump_D3DKMDT_VIDPN_SOURCE_MODE(pinnedMode, 0, "<Pinned>");
		}
	}

	while (status == STATUS_SUCCESS)
	{
		ASSERT(sourceMode);

		if (index < Global.nMaxSourceModeSetDump) {
			Dump_D3DKMDT_VIDPN_SOURCE_MODE(sourceMode, index, NULL);
		}
			
		++index;

		status = sourceModeSetInterface->pfnAcquireNextModeInfo(
			hSourceModeSet, sourceMode, &nextSourceMode
		);
		sourceModeSetInterface->pfnReleaseModeInfo(hSourceModeSet, sourceMode);
		sourceMode = nextSourceMode;
	}

Cleanup:
	if (pinnedMode) {
		sourceModeSetInterface->pfnReleaseModeInfo(hSourceModeSet, pinnedMode);
	}

	VidPnInterface->pfnReleaseSourceModeSet(hVidPn, hSourceModeSet);
}

const char* D3DKMDT_VIDEO_SIGNAL_STANDARD_Name(D3DKMDT_VIDEO_SIGNAL_STANDARD signal)
{
	switch (signal) {
	case D3DKMDT_VSS_UNINITIALIZED: return "Uninitialized";
	case D3DKMDT_VSS_VESA_DMT     : return "Vesa_DMT";
	case D3DKMDT_VSS_VESA_GTF     : return "Vesa_GTF";
	case D3DKMDT_VSS_VESA_CVT     : return "Vesa_CVT";
	case D3DKMDT_VSS_IBM          : return "IBM";
	case D3DKMDT_VSS_APPLE        : return "Apple";
	case D3DKMDT_VSS_NTSC_M       : return "Ntsc_M";
	case D3DKMDT_VSS_NTSC_J       : return "Ntsc_J";
	case D3DKMDT_VSS_NTSC_443     : return "Ntsc_443";
	case D3DKMDT_VSS_PAL_B        : return "Pal_B";
	case D3DKMDT_VSS_PAL_B1       : return "Pal_B1";
	case D3DKMDT_VSS_PAL_G        : return "Pal_G";
	case D3DKMDT_VSS_PAL_H        : return "Pal_H";
	case D3DKMDT_VSS_PAL_I        : return "Pal_I";
	case D3DKMDT_VSS_PAL_D        : return "Pal_D";
	case D3DKMDT_VSS_PAL_N        : return "Pal_N";
	case D3DKMDT_VSS_PAL_NC       : return "Pal_NC";
	case D3DKMDT_VSS_SECAM_B      : return "Secam_B";
	case D3DKMDT_VSS_SECAM_D      : return "Secam_D";
	case D3DKMDT_VSS_SECAM_G      : return "Secam_G";
	case D3DKMDT_VSS_SECAM_H      : return "Secam_H";
	case D3DKMDT_VSS_SECAM_K      : return "Secam_K";
	case D3DKMDT_VSS_SECAM_K1     : return "Secam_K1";
	case D3DKMDT_VSS_SECAM_L      : return "Secam_L";
	case D3DKMDT_VSS_SECAM_L1     : return "Secam_L1";
	case D3DKMDT_VSS_EIA_861      : return "Eia_861";
	case D3DKMDT_VSS_EIA_861A     : return "Eia_861A";
	case D3DKMDT_VSS_EIA_861B     : return "Eia_861B";
	case D3DKMDT_VSS_OTHER        : return "Other";
	default                       : return "<unknown>";
	}
}

const char* D3DDDI_VIDEO_SIGNAL_SCANLINE_ORDERING_Name(D3DDDI_VIDEO_SIGNAL_SCANLINE_ORDERING ordering)
{
	switch (ordering) {
	case D3DDDI_VSSLO_UNINITIALIZED: return "Uninitialized";
	case D3DDDI_VSSLO_PROGRESSIVE: return "Progressive";
	case D3DDDI_VSSLO_INTERLACED_UPPERFIELDFIRST: return "InterlacedUpperFieldFirst";
	case D3DDDI_VSSLO_INTERLACED_LOWERFIELDFIRST: return "InterlacedLowerFieldFirst";
	case D3DDDI_VSSLO_OTHER: return "Other";
	default: return "<unknown>";
	}
}

const char* D3DKMDT_MODE_PREFERENCE_Name(D3DKMDT_MODE_PREFERENCE pref)
{
	switch (pref) {
	case D3DKMDT_MP_UNINITIALIZED: return "Uninitialized";
	case D3DKMDT_MP_PREFERRED: return "Preferred";
	case D3DKMDT_MP_NOTPREFERRED: return "NotPrefered";
	default: return "<unknown>";
	}
}

void Dump_D3DKMDT_VIDPN_TARGET_MODE(
	const D3DKMDT_VIDPN_TARGET_MODE* TargetMode,
	DWORD index,
	const char* hints
)
{
	pr_info("TargetMode [%d] %s {\n", index, hints ? hints : "");
	pr_info("    VideoSignalInfo {\n");
	pr_info("        VideoStandard: %s\n", D3DKMDT_VIDEO_SIGNAL_STANDARD_Name(TargetMode->VideoSignalInfo.VideoStandard));
	pr_info("        TotalSize: %d-%d\n",
		TargetMode->VideoSignalInfo.TotalSize.cx,
		TargetMode->VideoSignalInfo.TotalSize.cy
	);
	pr_info("        ActiveSize: %d-%d\n",
		TargetMode->VideoSignalInfo.ActiveSize.cx,
		TargetMode->VideoSignalInfo.ActiveSize.cy
	);
	pr_info("        VSyncFreq: %d/%d\n",
		TargetMode->VideoSignalInfo.VSyncFreq.Numerator,
		TargetMode->VideoSignalInfo.VSyncFreq.Denominator
	);
	pr_info("        HSyncFreq: %d/%d\n",
		TargetMode->VideoSignalInfo.HSyncFreq.Numerator,
		TargetMode->VideoSignalInfo.HSyncFreq.Denominator
	);
	pr_info("        PixelRate: %d\n", TargetMode->VideoSignalInfo.PixelRate);
	pr_info("        ScanLineOrdering: %s\n",
		D3DDDI_VIDEO_SIGNAL_SCANLINE_ORDERING_Name(TargetMode->VideoSignalInfo.ScanLineOrdering));
	pr_info("    }\n");
	pr_info("    Preference: %s\n", D3DKMDT_MODE_PREFERENCE_Name(TargetMode->Preference));
	pr_info("}\n");
}

void Dump_TargetModeSet(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId
)
{
	D3DKMDT_HVIDPNTARGETMODESET hTargetModeSet;
	const DXGK_VIDPNTARGETMODESET_INTERFACE* targetModeSetInterface;
	D3DKMDT_VIDPN_TARGET_MODE* targetMode = NULL;
	D3DKMDT_VIDPN_TARGET_MODE* nextTargetMode;
	D3DKMDT_VIDPN_TARGET_MODE* pinnedMode = NULL;
	NTSTATUS status;
	DWORD index = 0;
	SIZE_T num;

	status = VidPnInterface->pfnAcquireTargetModeSet(
		hVidPn,
		VidPnTargetId,
		&hTargetModeSet,
		&targetModeSetInterface
	);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnAcquireTargetModeSet() failed, status(0x%08x)\n", status);
		return;
	}

	status = targetModeSetInterface->pfnGetNumModes(hTargetModeSet, &num);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnGetNumModes() failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	pr_info("NumTargetModes: %d\n", num);

	if (num == 0)
		goto Cleanup;

	if (Global.fDumpPinnedTargetMode) {
		status = targetModeSetInterface->pfnAcquirePinnedModeInfo(
			hTargetModeSet, &pinnedMode
		);
		if (status == STATUS_SUCCESS) {
			ASSERT(pinnedMode);

			Dump_D3DKMDT_VIDPN_TARGET_MODE(pinnedMode, 0, "<Pinned>");
		}
	}

	status = targetModeSetInterface->pfnAcquireFirstModeInfo(
		hTargetModeSet, &targetMode
	);
	if (status != STATUS_SUCCESS) {
		pr_err("pfnAcquireFirstModeInfo() failed, status(0x%08x)\n", status);
		goto Cleanup;
	}

	while (status == STATUS_SUCCESS) 
	{
		ASSERT(targetMode);

		if (index < Global.nMaxTargetModeSetDump ||
			(Global.fDumpPreferredTargetMode && targetMode->Preference == D3DKMDT_MP_PREFERRED))
		{
			Dump_D3DKMDT_VIDPN_TARGET_MODE(targetMode, index, NULL);
		}
	
		index++;

		status = targetModeSetInterface->pfnAcquireNextModeInfo(
			hTargetModeSet, targetMode, &nextTargetMode
		);
		targetModeSetInterface->pfnReleaseModeInfo(hTargetModeSet, targetMode);
		targetMode= nextTargetMode;
	}

Cleanup:
	if (pinnedMode) {
		targetModeSetInterface->pfnReleaseModeInfo(hTargetModeSet, pinnedMode);
	}

	VidPnInterface->pfnReleaseTargetModeSet(hVidPn, hTargetModeSet);
}



void Dump_VidPnPath(
	D3DKMDT_HVIDPN hVidPn,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DKMDT_HVIDPNTOPOLOGY hTopology,
	DXGK_VIDPNTOPOLOGY_INTERFACE* TopologyInterface
)
{
	NTSTATUS status;
	D3DKMDT_VIDPN_PRESENT_PATH* vidPnPathInfo;
	D3DKMDT_VIDPN_PRESENT_PATH* vidPnNextPathInfo;
	int nPath = 0;

	status = TopologyInterface->pfnAcquireFirstPathInfo(
		hTopology, &vidPnPathInfo
	);
	if (!NT_SUCCESS(status)) {
		pr_err("pfnAcquireFirstPathInfo() failed, error(0x%08x)\n", status);
		return;
	}

	while (status == STATUS_SUCCESS) {
		pr_info("Path [%d] {\n", nPath++);
		pr_info("    VidPnSourceId: %d\n", vidPnPathInfo->VidPnSourceId);
		pr_info("    VidPnTargetId: %d\n", vidPnPathInfo->VidPnTargetId);
		pr_info("    ImportanceOrdinal: %d\n", vidPnPathInfo->ImportanceOrdinal);
		pr_info("    ContentTransformation {\n");
		pr_info(
			"        Scaling: %s\n"
			"        ScalingSupport: %s\n"
			"        Rotation: %s\n"
			"        RotationSupport: %s\n",
			D3DKMDT_VIDPN_PRESENT_PATH_SCALING_Name(vidPnPathInfo->ContentTransformation.Scaling),
			D3DKMDT_VIDPN_PRESENT_PATH_SCALING_SUPPORT_Name(vidPnPathInfo->ContentTransformation.ScalingSupport),
			D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_Name(vidPnPathInfo->ContentTransformation.Rotation),
			D3DKMDT_VIDPN_PRESENT_PATH_ROTATION_SUPPORT_Name(vidPnPathInfo->ContentTransformation.RotationSupport)
			);
		pr_info("    }\n");
		pr_info("    VisibleFromActiveTLOffset: %d-%d\n",
			vidPnPathInfo->VisibleFromActiveTLOffset.cx,
			vidPnPathInfo->VisibleFromActiveTLOffset.cy
			);
		pr_info("    VisibleFromActiveBROffset: %d-%d\n",
			vidPnPathInfo->VisibleFromActiveBROffset.cx,
			vidPnPathInfo->VisibleFromActiveBROffset.cy);
		pr_info("    VidPnTargetColorBasis: %s\n",
			D3DKMDT_COLOR_BASIS_Name(vidPnPathInfo->VidPnTargetColorBasis));
		pr_info("}\n");

		if (Global.fDumpSourceModeSet) {
			Dump_SourceModeSet(hVidPn, VidPnInterface, vidPnPathInfo->VidPnSourceId);
		}

		if (Global.fDumpTargetModeSet) {
			Dump_TargetModeSet(hVidPn, VidPnInterface, vidPnPathInfo->VidPnTargetId);
		}
		status = TopologyInterface->pfnAcquireNextPathInfo(
			hTopology, vidPnPathInfo, &vidPnNextPathInfo
		);

		TopologyInterface->pfnReleasePathInfo(hTopology, vidPnPathInfo);
		vidPnPathInfo = vidPnNextPathInfo;
	}
}

void Dump_VidPnTopology(
	const DXGKRNL_INTERFACE* DxgkInterface,
	const DXGK_VIDPN_INTERFACE* VidPnInterface,
	D3DKMDT_HVIDPN hVidPn
)
{
	D3DKMDT_HVIDPNTOPOLOGY hTopology;
	DXGK_VIDPNTOPOLOGY_INTERFACE* topologyInterface;
	NTSTATUS status;
	SIZE_T nPath;

	status = VidPnInterface->pfnGetTopology(
		hVidPn, &hTopology, &topologyInterface
	);
	if (!NT_SUCCESS(status)) {
		pr_err("pfnGetTopology() failed, status(0x%08x)\n", status);
		return;
	}

	status = topologyInterface->pfnGetNumPaths(hTopology, &nPath);
	if (!NT_SUCCESS(status)) {
		pr_err("pfnGetNumPaths() failed, error(0x%08x)\n", status);
	}
	else {
		pr_info("NumPath: %d\n", nPath);
	}

	Dump_VidPnPath(hVidPn, VidPnInterface, hTopology, topologyInterface);
}

void Dump_VidPn(PWDDM_ADAPTER WddmAdapter, D3DKMDT_HVIDPN hVidPn)
{
	PDXGKRNL_INTERFACE dxgkInterface = &WddmAdapter->DxgkInterface;
	const DXGK_VIDPN_INTERFACE* VidPnInterface;
	NTSTATUS status;

	if (hVidPn == NULL) {
		pr_info("hVidPn is NULL\n");
		return;
	}

	if (Global.nVidPnDumped++ > Global.nMaxVidPnDump)
		return;

	status = dxgkInterface->DxgkCbQueryVidPnInterface(
		hVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &VidPnInterface
	);
	if (!NT_SUCCESS(status)) {
		pr_err("DxgkCbQueryVidPnInterface() failed, status(0x%08x)\n", status);
		return;
	}

	Dump_VidPnTopology(dxgkInterface, VidPnInterface, hVidPn);
}

const char* D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE_Name(D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE type)
{
	switch (type) {
	case D3DKMDT_EPT_UNINITIALIZED: return "Uninitialized";
	case D3DKMDT_EPT_VIDPNSOURCE  : return "VidPnSource";
	case D3DKMDT_EPT_VIDPNTARGET  : return "VidPnTarget";
	case D3DKMDT_EPT_SCALING      : return "Scaling";
	case D3DKMDT_EPT_ROTATION     : return "Rotation";
	case D3DKMDT_EPT_NOPIVOT      : return "NoPivot";
	default                       : return "<unknown>";
	}
}

void Dump_DXGKARG_ENUMVIDPNCOFUNCMODALITY(PWDDM_ADAPTER wddmAdapter, const DXGKARG_ENUMVIDPNCOFUNCMODALITY* const EnumCofuncModality)
{
	pr_info("DXGKARG_ENUMVIDPNCOFUNCMODALITY {\n");
	Dump_VidPn(wddmAdapter, EnumCofuncModality->hConstrainingVidPn);
	pr_info("    EnumPivotType: %s\n",
		D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE_Name(EnumCofuncModality->EnumPivotType));
	pr_info("    EnumPivot { VidPnSourceId(%d), VidPnTargetId(%d) }\n",
		EnumCofuncModality->EnumPivot.VidPnSourceId,
		EnumCofuncModality->EnumPivot.VidPnTargetId);
	pr_info("}\n");
}
