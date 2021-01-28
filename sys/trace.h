#ifndef TRACE_INC
#define TRACE_INC

#include <ntddk.h>
#include <ntstrsafe.h>

enum {
	LEVEL_ERROR,
	LEVEL_WARNING,
	LEVEL_INFO,
	LEVEL_TRACE,
};

#if DBG
#  define DEFAULT_LOG_LEVEL		LEVEL_TRACE
#else
#  define DEFAULT_LOG_LEVEL		LEVEL_WARNING
#endif 

extern int DebugLevel;

void TraceIndent();
void TraceUnindent();
void TraceOutput(int level, const char* fmt, ...);
void TraceDump(const void* buf, unsigned int buflen, const char* prefix);

#define pr_err(...)		do { TraceOutput(LEVEL_ERROR, __VA_ARGS__); } while (0)
#define pr_info(...)	do { TraceOutput(LEVEL_INFO, __VA_ARGS__); } while (0)
#define pr_debug(...)	do { TraceOutput(LEVEL_TRACE, __VA_ARGS__); } while (0)

extern const char* MajorFunctionStr[IRP_MJ_MAXIMUM_FUNCTION + 1];
const char* DxgkrnlVersionStr(ULONG ver);

#endif /* TRACE_INC */
/* end of file */
