#include <stdio.h>
#include <stdarg.h>

#include "log.h"

// for now log everything at startup (this needs a NV setup)
static int log_level = LOG_VERBOSE;

void set_log_level(int loglvl)
{
	log_level = loglvl;
}

const char *log_level_name(int loglvl)
{
	switch(loglvl) {
	case LOG_FATAL: return "FATAL";
	case LOG_ERROR: return "ERROR";
	case LOG_INFO:  return "INFO";
	case LOG_WARN:	return "WARN";
	case LOG_DEBUG:	return "DEBUG";
	case LOG_VERBOSE: return "VERBOSE";
	default: return "STRANGE";
	}
}

/* simply use printf for now */
void log(int loglvl, const char* str, ...)
{
    va_list arg;

	if (loglvl <= log_level) {
	    va_start(arg, str);
	    printf("%s: ", log_level_name(loglvl));
		vprintf(str, arg);
		printf("\n");
	    va_end(arg);
	}
}
