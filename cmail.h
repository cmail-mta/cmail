#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <sqlite3.h>

#define VERSION "cmail 0.1"

typedef struct /*_ARGS*/ {
	char* config_file;
	bool drop_privileges;
	bool detach;
} ARGUMENTS;

#include "arguments.c"
#include "conffile.c"
