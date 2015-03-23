#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../lib/logger.h"
#include "../lib/signal.h"

#include "../lib/logger.c"
#include "../lib/signal.c"
#include "../lib/daemonize.c"

#define VERSION "cmail-popd 0.1"

typedef struct /*_ARGS_COMPOSITE*/ {
	bool drop_privileges;
	bool daemonize;
	char* config_file;
} ARGUMENTS;

typedef struct /*_CONFIGURATION_DATA*/ {
	LOGGER log;
} CONFIGURATION;

#include "args.c"
#include "config.c"
