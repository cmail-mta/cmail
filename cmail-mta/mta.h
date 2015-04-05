#include <stdio.h>
#include <stdbool.h>

#include "../lib/common.h"

#define VERSION "cmail-mta 0.1"
#define CMAIL_MTA_INTERVAL 10

#include "../lib/logger.h"
#include "../lib/signal.h"
#include "../lib/privileges.h"
#include "../lib/config.h"
#include "../lib/tls.h"
#include "../lib/database.h"

#include "../lib/logger.c"
#include "../lib/signal.c"
#include "../lib/privileges.c"
#include "../lib/config.c"
#include "../lib/database.c"
#include "../lib/daemonize.c"

typedef struct /*_OUTBOUND_MAIL*/ {
	int* ids;
	char* remote;
	char* mailhost;
	char** recipients;
	int length;
	char* data;
} MAIL;

typedef struct /*_DATABASE_CONN*/ {
	sqlite3* conn;
	sqlite3_stmt* query_outbound;
} DATABASE;

typedef struct /*_ARGUMENT_COLLECTION*/ {
	bool drop_privileges;
	bool daemonize;
	char* config_file;
} ARGUMENTS;

typedef struct /*_CONFIGURATION_AGGREG*/ {
	DATABASE database;
	LOGGER log;
	USER_PRIVS privileges;
} CONFIGURATION;

#include "args.c"
#include "database.c"
#include "config.c"
#include "mail.c"
#include "coreloop.c"
