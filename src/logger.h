#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdio.h>
#include <string.h>

#define LOG_FATAL    (1)
#define LOG_ERR      (2)
#define LOG_WARN     (3)
#define LOG_INFO     (4)
#define LOG_DBG      (5)

extern FILE *logStream;
extern int  logLevel;

void LOG(int level, char *format, ...);

#endif
