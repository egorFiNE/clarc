#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void LOG(int level, char *format, ...) {
	if (level <= logLevel) { 
		char newFormat[1024*10];
		sprintf(newFormat, "%s\n", format);

		va_list args;
		va_start(args, format);
		vfprintf(logStream, newFormat, args);
		va_end(args);

		fflush(logStream); 
	} 
}

