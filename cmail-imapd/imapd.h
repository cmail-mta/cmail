#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>

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

typedef enum /*_AUTH_METHOD*/ {
	IMAP_AUTHENTICATE,
	IMAP_LOGIN
} AUTH_METHOD;

typedef enum /*_IMAP_STATE*/ {
	STATE_NEW,
	STATE_AUTHENTICATED,
	STATE_SELECTED
} IMAP_STATE;

typedef enum /*_IMAP_COMMAND_STATE*/ {
	COMMAND_UNHANDLED = 2,
	COMMAND_NOREPLY = 1,
	COMMAND_OK = 0,
	COMMAND_BAD = -1
} IMAP_COMMAND_STATE;

typedef struct /*_AUTHENTICATION_DATA*/ {
	AUTH_METHOD method;

	SASL_USER user;
	SASL_CONTEXT ctx;
} AUTH_DATA;

typedef struct /*_LISTEN_DATA*/ {
	bool tls_require;
	#ifndef CMAIL_NO_TLS
	gnutls_certificate_credentials_t tls_cert;
	gnutls_priority_t tls_priorities;
	gnutls_dh_params_t tls_dhparams;
	#endif
} LISTENER;

typedef struct /*_IMAP_COMMAND*/ {
	char* backing_buffer;
	size_t backing_buffer_length;
	
	char* tag;
	char* command;
	char* parameters;
} IMAP_COMMAND;

typedef struct /*_CLIENT_DATA*/ {
	CONNECTION* listener;
	IMAP_STATE state;
	AUTH_DATA auth;
	IMAP_COMMAND sentence;

	char recv_buffer[CMAIL_RECEIVE_BUFFER_LENGTH];
	size_t recv_offset;
	
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
int client_close(LOGGER log, CONNECTION* client, DATABASE* database);
int client_send(LOGGER log, CONNECTION* client, char* fmt, ...);
#ifndef CMAIL_NO_TLS
int client_starttls(LOGGER log, CONNECTION* client);
#endif

#include "arguments.c"
#include "database.c"
#include "config.c"
#include "auth.c"
#include "imapstatemachine.c"
#include "client.c"
#include "coreloop.c"
