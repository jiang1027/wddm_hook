#ifndef TRACE_INC
#define TRACE_INC

#include <ntddk.h>

enum {
	LEVEL_ERROR,
	LEVEL_WARNING,
	LEVEL_INFO,
	LEVEL_TRACE,
};

#if DBG
#  define LOG_LEVEL	LEVEL_TRACE
#else 
#  define LOG_LEVEL	LEVEL_WARNING
#endif 

#define Trace(level, ...) \
	do { if ((level) <= LOG_LEVEL) { DbgPrint(__VA_ARGS__); } } while (0)

// void Trace(ULONG Flag, PCCHAR Message, ...);
void TraceDump(const void* buf, unsigned int buflen, const char* prefix);

#define pr_err(...)		do { Trace(LEVEL_ERROR, __VA_ARGS__); } while (0)
#define pr_info(...)	do { Trace(LEVEL_INFO, __VA_ARGS__); } while (0)
#define pr_debug(...)	do { Trace(LEVEL_TRACE, __VA_ARGS__); } while (0)

extern const char* MajorFunctionStr[IRP_MJ_MAXIMUM_FUNCTION + 1];
const char* DxgkrnlVersionStr(ULONG ver);

#endif /* TRACE_INC */
/* end of file */
