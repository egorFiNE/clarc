#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

void LOG(int level, char *format, ...) {
	if (level <= logLevel) { 
		char *newFormat = malloc(strlen(format)+2);
		strcpy(newFormat, format);
		strcat(newFormat, "\n");

		va_list args;
		va_start(args, format);
		vfprintf(logStream, newFormat, args);
		va_end(args);

		fflush(logStream); 
		free(newFormat);
	} 
}

