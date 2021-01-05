#ifndef COMMON_INC
#define COMMON_INC

#include "stdafx.h"

enum {
	LOG_LEVEL_ERROR = 0,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_INFO,
	LOG_LEVEL_TRACE,
};

extern int trace_level;
extern int trace_dump_level;

void trace(int level, const char* fmt, ...);
void trace_dump(void* buf, int buflen, const char* prefix);

#define pr_err(...)		do { trace(LOG_LEVEL_ERROR, __VA_ARGS__);	} while (0)
#define pr_info(...)	do { trace(LOG_LEVEL_INFO, __VA_ARGS__);	} while (0)
#define pr_debug(...)	do { trace(LOG_LEVEL_TRACE, __VA_ARGS__);	} while (0)


#endif /* COMMON_INC */
