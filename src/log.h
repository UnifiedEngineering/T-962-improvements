#ifndef _LOG_H
#define _LOG_H

// Define logging levels
#define LOG_NONE	-9		// only for set_level to completely turn off logging
#define LOG_FATAL	-2		// A fatal error has occurred: program will block
#define LOG_ERROR	-1		// An error has occurred: program may not exit
#define LOG_WARN	0		// Any circumstance that may not affect normal operation
#define LOG_INFO	1		// Necessary information regarding program operation
#define LOG_DEBUG	2		// Standard debug messages
#define LOG_VERBOSE	3		// All debug messages

void log(int loglvl, const char* str, ...) __attribute__((format (printf, 2, 3)));
void set_log_level(int loglvl);

#endif
