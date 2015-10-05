#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>

#include "imaplimits.h"

#include "../lib/common.h"
#define VERSION 			"cmail-imapd 0.1"
#define CMAIL_SELECT_TIMEOUT		20
#define CMAIL_FAILSCORE_LIMIT		-10

#define CMAIL_HAVE_LISTENER_TYPE
#define CMAIL_HAVE_DATABASE_TYPE

#include "../lib/logger.h"
#include "../lib/network.h"
#include "../lib/connpool.h"
#include "../lib/signal.h"
#include "../lib/privileges.h"
#include "../lib/config.h"
#include "../lib/sasl.h"
#include "../lib/database.h"

#include "../lib/common.c"
#include "../lib/logger.c"
#include "../lib/network.c"
#include "../lib/connpool.c"
#include "../lib/signal.c"
#include "../lib/privileges.c"
#include "../lib/config.c"
#include "../lib/daemonize.c"
#include "../lib/database.c"

typedef enum /*_IMAP_STATE*/ {
	STATE_NEW,
	STATE_AUTHENTICATED,
	STATE_SELECTED
} IMAP_STATE;

typedef struct /*_LISTEN_DATA*/ {
	char* announce_domain;
	bool tls_require;
	#ifndef CMAIL_NO_TLS
	gnutls_certificate_credentials_t tls_cert;
	gnutls_priority_t tls_priorities;
	gnutls_dh_params_t tls_dhparams;
	#endif
} LISTENER;

typedef struct /*_CLIENT_DATA*/ {
	CONNECTION* listener;
	IMAP_STATE state;
	char recv_buffer[CMAIL_RECEIVE_BUFFER_LENGTH];
	size_t recv_offset;
	SASL_USER sasl_user;
	SASL_CONTEXT sasl_context;
	time_t last_action;
	int connection_score;
} CLIENT;

typedef struct /*_ARGS*/ {
	char* config_file;
	bool drop_privileges;
	bool detach;
} ARGUMENTS;

typedef struct /*_DATABASE_CONNECTION*/ {
	sqlite3* conn;
	sqlite3_stmt* query_authdata;
	sqlite3_stmt* query_userdatabase;
	sqlite3_stmt* fetch_master; 
	sqlite3_stmt* db_attach; 
	sqlite3_stmt* db_detach; 
} DATABASE;

typedef struct /*_CONF_META*/ {
	CONNPOOL listeners;
	USER_PRIVS privileges;
	LOGGER log;
	DATABASE database;
	char* pid_file;
} CONFIGURATION;

//These need some defined types
#include "../lib/auth.h"
#include "../lib/auth.c"
#include "../lib/sasl.c"
#include "../lib/client.c"
#ifndef CMAIL_NO_TLS
#include "../lib/tls.c"
#endif

//PROTOTYPES
int client_close(CONNECTION* client);
int client_send(LOGGER log, CONNECTION* client, char* fmt, ...);
#ifndef CMAIL_NO_TLS
int client_starttls(LOGGER log, CONNECTION* client);
#endif

#include "arguments.c"
#include "database.c"
#include "config.c"
