#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include <fcntl.h>

#include "../lib/common.h"
#define VERSION "cmail-popd 0.1"
#define CMAIL_MAX_CONCURRENT_CLIENTS 128
#define STATIC_SEND_BUFFER_LENGTH 1024

#include "../lib/logger.h"
#include "../lib/signal.h"
#include "../lib/privileges.h"
#include "../lib/config.h"
#include "../lib/network.h"
#include "../lib/connpool.h"
#include "../lib/auth.h"

#ifndef CMAIL_NO_TLS
#include "../lib/tls.h"
#endif

#include "../lib/logger.c"
#include "../lib/signal.c"
#include "../lib/privileges.c"
#include "../lib/config.c"
#include "../lib/network.c"
#include "../lib/connpool.c"

#include "../lib/daemonize.c"
#include "../lib/database.c"

#include "poplimits.h"

typedef struct /*_MAIL_ENTRY*/ {
	int database_id;
	int mail_size;
	bool flag_master;
	bool flag_delete;
} POP_MAIL;

typedef struct /*_MAILDROP_DESC*/ {
	unsigned count;
	POP_MAIL* mails;
	sqlite3_stmt* list_user;
	sqlite3_stmt* fetch_user;
	sqlite3_stmt* delete_user;
} MAILDROP;

typedef enum /*_AUTH_METHOD*/ {
	AUTH_USER,
	AUTH_SASLPLAIN
} AUTH_METHOD;

typedef enum /*_POP_STATE*/ {
	STATE_AUTH,
	STATE_TRANSACTION,
	STATE_UPDATE
} POPSTATE;

typedef struct /*_AUTHENTICATION_DATA*/ {
	AUTH_METHOD method;
	char* user;
	bool authorized;
} AUTH_DATA;

typedef struct /*_DATABASE_CONNECTION*/ {
	sqlite3* conn;
	sqlite3_stmt* query_authdata;
	sqlite3_stmt* query_lock;
	sqlite3_stmt* update_lock;
	sqlite3_stmt* list_master;
	sqlite3_stmt* fetch_master;
	sqlite3_stmt* delete_master;
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

typedef struct /*_CLIENT_DATA*/ {
	CONNECTION* listener;
	char recv_buffer[POP_MAX_LINE_LENGTH];
	unsigned recv_offset;
	POPSTATE state;
	AUTH_DATA auth;
	MAILDROP maildrop;
	#ifndef CMAIL_NO_TLS
	gnutls_session_t tls_session;
	TLSMODE tls_mode;
	#endif
} CLIENT;

typedef struct /*_POP3_LISTENER*/ {
	char* announce_domain;
	#ifndef CMAIL_NO_TLS
	bool tls_require;
	TLSMODE tls_mode;
	gnutls_certificate_credentials_t tls_cert;
	gnutls_priority_t tls_priorities;
	gnutls_dh_params_t tls_dhparams;
	#endif
} LISTENER;

//These need some defined types
#include "../lib/auth.c"
#ifndef CMAIL_NO_TLS
#include "../lib/tls.c"
#endif

int client_close(CONNECTION* client);
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
