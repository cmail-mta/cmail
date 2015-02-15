#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include <sqlite3.h>

#define VERSION "cmail 0.1"

volatile unsigned int verbosity=0;

typedef struct /*_ARGS*/ {
	char* config_file;
	bool drop_privileges;
	bool detach;
} ARGUMENTS;

typedef struct /*_LISTEN_FDS*/ {
	unsigned count;
	int* fds;
} LISTENERS;

typedef struct /*_CONF_META*/ {
	LISTENERS listeners;
	struct {int uid; int gid;} privileges;
	sqlite3* master;
	int logfd;
} CONFIGURATION;

#include "arguments.c"
#include "config.c"
