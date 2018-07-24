/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#define LOGGER_TIMESTRING_LEN 80

#define LOG_ERROR 	0
#define LOG_WARNING 	0
#define LOG_INFO 	1
#define LOG_DEBUG 	3
#define LOG_ALL_IO	4

void logprintf(unsigned level, char* fmt, ...);
void log_dump_buffer(unsigned level, void* buffer, size_t bytes);

void log_verbosity(int level, bool secondary);
FILE* log_output(FILE* stream);

int log_start();
void log_shutdown();
