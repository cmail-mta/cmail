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
#include <pthread.h>

#include "imaplimits.h"

#include "../lib/common.h"
#define VERSION 			"cmail-imapd 0.1"
#define CMAIL_SELECT_TIMEOUT		20
#define CMAIL_FAILSCORE_LIMIT		-10
#define FEEDBACK_PIPE_BUFFER		20

#define CMAIL_HAVE_LISTENER_TYPE
#define CMAIL_HAVE_DATABASE_TYPE
#define LOGGER_MT_SAFE

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

typedef enum /*_IMAP_COMMAND_STATE*/ {
	COMMAND_UNHANDLED = 9001,
	COMMAND_NO = 2,
	COMMAND_NOREPLY = 1,
	COMMAND_OK = 0,
	COMMAND_BAD = -1
} IMAP_COMMAND_STATE;

typedef enum /*_QUEUED_COMMAND_STATE*/ {
	COMMAND_NEW,
	COMMAND_IN_PROGRESS,
	COMMAND_REPLY,
	COMMAND_CANCELED,
	COMMAND_CANCEL_ACK,
	COMMAND_SYSTEM,
	COMMAND_INTERNAL_FAILURE
} QUEUED_COMMAND_STATE;

typedef struct _QUEUED_COMMAND {
	bool active;
	time_t last_enqueue;
	struct _QUEUED_COMMAND* next;
	struct _QUEUED_COMMAND* prev;

	CONNECTION* client;

	QUEUED_COMMAND_STATE queue_state;
	bool discard;
	char* backing_buffer;
	size_t backing_buffer_length;

	char* tag;
	char* command;
	int* parameters;
	size_t parameters_length;

	char* replies;
	size_t replies_length;
} QUEUED_COMMAND;

typedef struct /*_COMMAND_QUEUE*/ {
	QUEUED_COMMAND* entries;
	size_t entries_length;
	QUEUED_COMMAND system_entries[CMAIL_MAX_CONCURRENT_CLIENTS];

	pthread_mutex_t queue_access;
	pthread_cond_t queue_dirty;
	QUEUED_COMMAND* tail;
	QUEUED_COMMAND* head;
} COMMAND_QUEUE;

typedef enum /*_AUTH_METHOD*/ {
	IMAP_AUTHENTICATE,
	IMAP_LOGIN
} AUTH_METHOD;

typedef enum /*_IMAP_STATE*/ {
	STATE_NEW,
	STATE_SASL,
	STATE_AUTHENTICATED
} IMAP_STATE;

typedef struct /*_AUTHENTICATION_DATA*/ {
	AUTH_METHOD method;
	char* auth_tag;

	SASL_USER user;
	SASL_CONTEXT ctx;
} AUTH_DATA;

typedef enum /*_AUTH_OFFER_MODE*/ {
	AUTH_ANY,
	AUTH_TLSONLY
} AUTH_OFFER;

typedef struct /*_LISTEN_DATA*/ {
	char* fixed_user;
	AUTH_OFFER auth_offer;
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
	unsigned recv_literal_bytes;

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
} DATABASE;

typedef struct /*_WORKER_DB_CONN*/ {
	sqlite3* conn;
	sqlite3_stmt* mailbox_find;
	sqlite3_stmt* mailbox_info;
	sqlite3_stmt* mailbox_create;
	sqlite3_stmt* mailbox_delete;
	sqlite3_stmt* query_userdatabase;
	sqlite3_stmt* fetch;
} WORKER_DATABASE;

typedef struct /*_WORKER_CLIENT_DATA*/ {
	CONNECTION* client;
	WORKER_DATABASE user_database;
	char* authorized_user;
	long selection_master;
	long selection_user;
	bool select_readwrite;
} WORKER_CLIENT;

typedef struct /*_CONF_META*/ {
	CONNPOOL listeners;
	USER_PRIVS privileges;
	LOGGER log;
	DATABASE database;
	char* pid_file;
} CONFIGURATION;

typedef struct /*_THREAD_PARAM*/ {
	LOGGER log;
	COMMAND_QUEUE* queue;
	int feedback_pipe[2];
	const char* master_db;
} THREAD_CONFIG;

//These need some defined types
#include "../lib/auth.h"
#include "../lib/auth.c"
#include "../lib/sasl.c"
#include "../lib/client.c"
#ifndef CMAIL_NO_TLS
#include "../lib/tls.c"
#endif

//PROTOTYPES
int client_close(LOGGER log, CONNECTION* client, DATABASE* database, COMMAND_QUEUE* command_queue);
int client_send(LOGGER log, CONNECTION* client, char* fmt, ...);
#ifndef CMAIL_NO_TLS
int client_starttls(LOGGER log, CONNECTION* client);
#endif

#include "arguments.c"
#include "database.c"
#include "config.c"
#include "auth.c"
#include "protocol.c"
#include "workerdata.c"
#include "commandqueue.c"
#include "imapcommands.c"
#include "imapstatemachine.c"
#include "client.c"
#include "queueworker.c"
#include "coreloop.c"
