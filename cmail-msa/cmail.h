#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>
#include <nettle/base16.h>
#include <nettle/sha2.h>

//#define CMAIL_NO_TLS
#ifndef CMAIL_NO_TLS
	#include <gnutls/gnutls.h>
	#define TLSSUPPORT(x) (x)
	typedef enum /*_TLS_MODE*/ {
		TLS_ONLY,		//Listener: TLS-Only port, Client: TLS Session active
		TLS_NEGOTIATE,		//Listener: STARTTLS enabled, Client: Handshake in progress
		TLS_NONE		//Listener: No TLS, Client: No TLS session active
	} TLSMODE;
#else
	#define TLSSUPPORT(x)
#endif

#include <sqlite3.h>

#define VERSION 			"cmail-msa 0.1"
#define MAX_CFGLINE 			2048
#define LISTEN_QUEUE_LENGTH 		128
#define STATIC_SEND_BUFFER_LENGTH	1024

#include "smtplimits.h"

#include "../lib/logger.h"
#include "../lib/network.h"
#include "../lib/connpool.h"

#include "../lib/logger.c"
#include "../lib/network.c"
#include "../lib/connpool.c"

typedef enum /*_AUTHENTICATION_OFFER_MODE*/ {
	AUTH_NONE,			//Authentication not supported
	AUTH_ANY,			//Authentication supported
	AUTH_TLSONLY			//Authentication only in TLS session
} AUTH_OFFER;

typedef enum /*_AUTHENTICATION_METHOD*/ {
	AUTH_PLAIN			//PLAIN only for now
} AUTH_METHOD;

typedef struct /*_AUTHENTICATION_DATA*/ {
	AUTH_METHOD method;
	char* user;

	char* parameter;
	char* challenge;
	char* response;
} AUTH_DATA;

typedef struct /*_MAIL_PATH*/ {
	bool in_transaction;
	char path[SMTP_MAX_PATH_LENGTH];
	char* resolved_user;				//HEAP'd
} MAILPATH;

typedef struct /*_MAIL_STRUCT*/ {
	MAILPATH reverse_path;
	MAILPATH* forward_paths[SMTP_MAX_RECIPIENTS];
	unsigned data_allocated;			//STACK'd, persistent
	unsigned data_offset;
	char* data;					//HEAP'd
	char* submitter;
} MAIL;

typedef enum /*_SMTP_STATE*/ {
	STATE_NEW,
	STATE_IDLE,
	STATE_AUTH,
	STATE_RECIPIENTS,
	STATE_DATA
} SMTPSTATE;

typedef struct /*_LISTEN_DATA*/ {
	char* announce_domain;
	bool auth_require;
	AUTH_OFFER auth_offer;
	#ifndef CMAIL_NO_TLS
	TLSMODE tls_mode;
	gnutls_certificate_credentials_t tls_cert;
	gnutls_priority_t tls_priorities;
	gnutls_dh_params_t tls_dhparams;
	#endif
} LISTENER;

typedef struct /*_CLIENT_DATA*/ {
	CONNECTION* listener;
	SMTPSTATE state;
	char recv_buffer[SMTP_MAX_LINE_LENGTH];
	unsigned recv_offset;
	char peer_name[MAX_FQDN_LENGTH];
	MAIL current_mail;
	AUTH_DATA auth;
	/*last_action*/
	#ifndef CMAIL_NO_TLS
	gnutls_session_t tls_session;
	TLSMODE tls_mode;
	#endif
} CLIENT;

typedef struct /*_PATH_POOL*/{
	unsigned count;
	MAILPATH** paths;
} PATHPOOL;

typedef struct /*_ARGS*/ {
	char* config_file;
	bool drop_privileges;
	bool detach;
} ARGUMENTS;

typedef struct /*_USER_MAILBOX_DB*/{
	bool active;
	sqlite3_stmt* mailbox;
	char* file_name;
	char* conn_handle;
} USER_DATABASE;

typedef struct /*_DATABASE_CONNECTION*/ {
	sqlite3* conn;
	sqlite3_stmt* query_authdata;
	sqlite3_stmt* query_addresses;
	sqlite3_stmt* query_inrouter;
	sqlite3_stmt* query_outrouter;
	struct {
		sqlite3_stmt* mailbox_master;
		sqlite3_stmt* outbox_master;
		USER_DATABASE** users;
	} mail_storage;
} DATABASE;

typedef struct /*_CONF_META*/ {
	CONNPOOL listeners;
	struct {int uid; int gid;} privileges;
	LOGGER log;
	DATABASE database;
} CONFIGURATION;

typedef struct /*_MAIL_ROUTE*/ {
	char* router;
	char* argument;
} MAILROUTE;

//PROTOTYPES
int client_close(CONNECTION* client);
int client_send(LOGGER log, CONNECTION* client, char* fmt, ...);
int mail_store_inbox(LOGGER log, sqlite3_stmt* stmt, MAIL* mail, MAILPATH* current_path);
int mail_store_outbox(LOGGER log, sqlite3_stmt* stmt, char* mail_remote, char* envelope_to, MAIL* mail);
#ifndef CMAIL_NO_TLS
int client_starttls(LOGGER log, CONNECTION* client);
#endif

volatile sig_atomic_t abort_signaled=0;

#include "signal.c"
#include "database.c"
#include "path.c"
#include "route.c"
#include "pathpool.c"
#include "mail.c"
#include "auth.c"
#include "smtpstatemachine.c"
#include "client.c"
#include "coreloop.c"

#include "arguments.c"
#include "config.c"
