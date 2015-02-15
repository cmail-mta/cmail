#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>

#include <sqlite3.h>

#define VERSION "cmail 0.1"
#define MAX_CFGLINE 2048

typedef struct /*_ARGS*/ {
	char* config_file;
	bool drop_privileges;
	bool detach;
} ARGUMENTS;

typedef struct /*_FD_COLLECTION*/ {
	unsigned count;
	int* fds;
} FDCOLLECTION;

typedef struct /*_LOGGER*/ {
	FILE* stream;
	unsigned verbosity;
} LOGGER;

typedef struct /*_CONF_META*/ {
	FDCOLLECTION listeners;
	struct {int uid; int gid;} privileges;
	LOGGER log;
	sqlite3* master;
} CONFIGURATION;

void logprintf(LOGGER log, unsigned level, char* fmt, ...);

#define LOG_ERROR 	0
#define LOG_WARNING 	0
#define LOG_INFO 	1
#define LOG_DEBUG 	3

#include "fdcollection.c"

#include "arguments.c"
#include "config.c"
