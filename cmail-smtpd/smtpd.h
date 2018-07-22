#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>

#include "../lib/common.h"
#define VERSION 			"cmail-smtpd 0.1"
#define CMAIL_SELECT_TIMEOUT		20
#define CMAIL_FAILSCORE_LIMIT		-10
#define CMAIL_MAX_HOPS			30
#define CMAIL_MEMORY_TIMEOUT		600

#define CMAIL_HAVE_LISTENER_TYPE

#include "smtplimits.h"

#include "../lib/logger.h"
#include "../lib/network.h"
#include "../lib/connpool.h"
#include "../lib/signal.h"
#include "../lib/privileges.h"
#include "../lib/config.h"
#include "../lib/sasl.h"
//#include "../lib/tls.h" //Pulled in by network.h anyway
#include "../lib/database.h"

#include "../lib/common.c"
#include "../lib/network.c"
#include "../lib/connpool.c"
#include "../lib/signal.c"
#include "../lib/privileges.c"
#include "../lib/config.c"
#include "../lib/daemonize.c"
#include "../lib/database.c"

typedef enum /*_AUTHENTICATION_OFFER_MODE*/ {
	AUTH_NONE,			//Authentication not supported
	AUTH_ANY,			//Authentication supported
	AUTH_TLSONLY			//Authentication only in TLS session
} AUTH_OFFER;

typedef struct /*_MAIL_ROUTE*/ {
	char* router;
	char* argument;
} MAILROUTE;

typedef struct /*_MAIL_PATH*/ {
	bool in_transaction;
	unsigned delimiter_position;
	char path[SMTP_MAX_PATH_LENGTH];
	MAILROUTE route;
} MAILPATH;

typedef struct /*_MAIL_STRUCT*/ {
	MAILPATH reverse_path;
	MAILPATH* forward_paths[SMTP_MAX_RECIPIENTS];
	unsigned data_allocated;			//STACK'd, persistent
	unsigned data_offset;
	char* data;					//HEAP'd
	char* submitter;				//Should always point to the submitter member of a CLIENT structure
	char* protocol;
	char message_id[CMAIL_MESSAGEID_MAX+1];
	unsigned data_max;
	unsigned hop_count;
	unsigned header_offset;
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
	char* fixed_user;
	unsigned max_size;
	bool auth_require;
	bool suppress_submitter;
	AUTH_OFFER auth_offer;
	#ifndef CMAIL_NO_TLS
	gnutls_certificate_credentials_t tls_cert;
	gnutls_priority_t tls_priorities;
	gnutls_dh_params_t tls_dhparams;
	#endif
} LISTENER;

typedef struct /*_CLIENT_DATA*/ {
	CONNECTION* listener;
	SMTPSTATE state;
	char recv_buffer[CMAIL_RECEIVE_BUFFER_LENGTH];
	size_t recv_offset;
	char peer_name[MAX_FQDN_LENGTH];
	MAIL current_mail;
	MAILROUTE originating_route;
	SASL_USER sasl_user;
	SASL_CONTEXT sasl_context;
	time_t last_action;
	int connection_score;
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
	sqlite3* conn;
	sqlite3_stmt* mailbox;
	char* file_name;
} USER_DATABASE;

typedef struct /*_DATABASE_CONNECTION*/ {
	sqlite3* conn;
	sqlite3_stmt* query_authdata;
	sqlite3_stmt* query_address;
	sqlite3_stmt* query_outbound_router;
	struct {
		sqlite3_stmt* mailbox_master;
		sqlite3_stmt* outbox_master;
		USER_DATABASE** users;
	} mail_storage;
} DATABASE;

typedef struct /*_CONF_META*/ {
	CONNPOOL listeners;
	USER_PRIVS privileges;
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
int client_send(CONNECTION* client, char* fmt, ...);
int mail_store_inbox(sqlite3_stmt* stmt, MAIL* mail, MAILPATH* current_path);
int mail_store_outbox(sqlite3_stmt* stmt, char* mail_remote, char* envelope_to, MAIL* mail);
#ifndef CMAIL_NO_TLS
int client_starttls(CONNECTION* client);
#endif

#include "database.c"
#include "route.c"
#include "path.c"
#include "pathpool.c"
#include "mail.c"
#include "smtpstatemachine.c"
#include "client.c"
#include "coreloop.c"

#include "arguments.c"
#include "config.c"
