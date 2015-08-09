#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#define LOGGER_TIMESTRING_LEN 80

typedef struct /*_LOGGER*/ {
	FILE* stream;
	unsigned verbosity;
	bool log_secondary;
	bool print_timestamp;
} LOGGER;

#define LOG_ERROR 	0
#define LOG_WARNING 	0
#define LOG_INFO 	1
#define LOG_DEBUG 	3
#define LOG_ALL_IO	4

void logprintf(LOGGER log, unsigned level, char* fmt, ...);
