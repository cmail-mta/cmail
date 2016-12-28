#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "../lib/common.h"
#define VERSION "cmail-popd 0.1"
#define CMAIL_MAX_CONCURRENT_CLIENTS 128
#define CMAIL_SELECT_TIMEOUT 20
#define CMAIL_FAILSCORE_LIMIT -5

#define CMAIL_HAVE_LISTENER_TYPE

#include "../lib/logger.h"
#include "../lib/signal.h"
#include "../lib/privileges.h"
#include "../lib/config.h"
#include "../lib/network.h"
#include "../lib/sasl.h"
#include "../lib/connpool.h"
//#include "../lib/tls.h" //Pulled in by network.h anyway
#include "../lib/database.h"

#include "../lib/common.c"
#include "../lib/logger.c"
#include "../lib/signal.c"
#include "../lib/privileges.c"
#include "../lib/config.c"
#include "../lib/network.c"
#include "../lib/connpool.c"
#include "../lib/database.c"

#include "../lib/daemonize.c"

#include "poplimits.h"

typedef struct /*_MAIL_ENTRY*/ {
	int database_id;
	int mail_size;
	bool flag_master;
	bool flag_delete;
	char message_id[POP_MESSAGEID_MAX + 1];
} POP_MAIL;

typedef struct /*_MAILDROP_DESC*/ {
	unsigned count;
	POP_MAIL* mails;

	sqlite3* user_conn;

	sqlite3_stmt* list_user;
	sqlite3_stmt* fetch_user;

	sqlite3_stmt* mark_deletion;
	sqlite3_stmt* unmark_deletions;
	sqlite3_stmt* delete_user;
} MAILDROP;

typedef enum /*_AUTH_METHOD*/ {
	AUTH_USER,
	AUTH_SASL
} AUTH_METHOD;

typedef enum /*_POP_STATE*/ {
	STATE_AUTH,
	STATE_TRANSACTION,
	STATE_UPDATE
} POPSTATE;

typedef struct /*_AUTHENTICATION_DATA*/ {
	AUTH_METHOD method;
	bool auth_ok;

	SASL_CONTEXT ctx;
	SASL_USER user;
} AUTH_DATA;

typedef struct /*_DATABASE_CONNECTION*/ {
	sqlite3* conn;

	sqlite3_stmt* query_authdata;
	sqlite3_stmt* query_userdatabase;
	sqlite3_stmt* update_lock;

	sqlite3_stmt* list_master;
	sqlite3_stmt* fetch_master;

	sqlite3_stmt* mark_deletion;
	sqlite3_stmt* unmark_deletions;
	sqlite3_stmt* delete_master;
} DATABASE;

typedef struct /*_ARGS_COMPOSITE*/ {
	bool drop_privileges;
	bool daemonize;
	char* config_file;
	bool test_config;
} ARGUMENTS;

typedef struct /*_CONFIGURATION_DATA*/ {
	CONNPOOL listeners;
	DATABASE database;
	LOGGER log;
	USER_PRIVS privileges;
	char* pid_file;
} CONFIGURATION;

typedef struct /*_CLIENT_DATA*/ {
	CONNECTION* listener;
	char recv_buffer[CMAIL_RECEIVE_BUFFER_LENGTH];
	size_t recv_offset;
	POPSTATE state;
	AUTH_DATA auth;
	MAILDROP maildrop;
	time_t last_action;
	int connection_score;
} CLIENT;

typedef struct /*_POP3_LISTENER*/ {
	char* announce_domain;
	#ifndef CMAIL_NO_TLS
	bool tls_require;
	gnutls_certificate_credentials_t tls_cert;
	gnutls_priority_t tls_priorities;
	gnutls_dh_params_t tls_dhparams;
	#endif
} LISTENER;

//These need some defined types
#include "../lib/auth.h"
#include "../lib/auth.c"
#include "../lib/sasl.c"
#include "../lib/client.c"
#ifndef CMAIL_NO_TLS
#include "../lib/tls.c"
#endif

int client_close(LOGGER log, CONNECTION* client, DATABASE* database);
int client_send(LOGGER log, CONNECTION* client, char* fmt, ...);
#ifndef CMAIL_NO_TLS
int client_starttls(LOGGER log, CONNECTION* client);
#endif

#include "database.c"
#include "maildrop.c"
#include "args.c"
#include "config.c"
#include "auth.c"
#include "popfunctions.c"
#include "popstatemachine.c"
#include "client.c"
#include "coreloop.c"
