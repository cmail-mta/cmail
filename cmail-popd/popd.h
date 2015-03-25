#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sqlite3.h>

#include "../lib/common.h"
#define VERSION "cmail-popd 0.1"

#include "../lib/logger.h"
#include "../lib/signal.h"
#include "../lib/privileges.h"
#include "../lib/config.h"
#include "../lib/network.h"
#include "../lib/connpool.h"

#include "../lib/logger.c"
#include "../lib/signal.c"
#include "../lib/privileges.c"
#include "../lib/config.c"
#include "../lib/network.c"
#include "../lib/connpool.c"

#include "../lib/daemonize.c"
#include "../lib/database.c"

typedef struct /*_DATABASE_CONNECTION*/ {
	sqlite3* conn;
} DATABASE;

typedef struct /*_ARGS_COMPOSITE*/ {
	bool drop_privileges;
	bool daemonize;
	char* config_file;
} ARGUMENTS;

typedef struct /*_CONFIGURATION_DATA*/ {
	CONNPOOL listeners;
	DATABASE database;
	LOGGER log;
	USER_PRIVS privileges;
} CONFIGURATION;

typedef struct /*_POP3_LISTENER*/ {
	char* announce_domain;
} LISTENER;

#include "database.c"
#include "args.c"
#include "config.c"
#include "coreloop.c"
