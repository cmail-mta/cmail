#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <adns.h>
#include <time.h>

#include "../lib/common.h"
#include "smtplimits.h"
#include "../cmail-msa/smtplimits.h"

#define VERSION "cmail-mta 0.1"
#define CMAIL_HAVE_DATABASE_TYPE

#include "../lib/logger.h"
#include "../lib/signal.h"
#include "../lib/privileges.h"
#include "../lib/config.h"
#include "../lib/tls.h"
#include "../lib/database.h"
#include "../lib/network.h"

#include "../lib/logger.c"
#include "../lib/signal.c"
#include "../lib/privileges.c"
#include "../lib/config.c"
#include "../lib/tls.c"
#include "../lib/database.c"
#include "../lib/daemonize.c"
#include "../lib/network.c"
#include "../lib/client.c"

typedef enum /*_DELIVERY_MODE*/ {
	DELIVER_DOMAIN,
	DELIVER_HANDOFF
} DELIVERY_MODE;

typedef enum /*_MAIL_STATUS*/ {
	RCPT_READY,
	RCPT_OK,
	RCPT_FAIL_TEMPORARY,
	RCPT_FAIL_PERMANENT
} RCPTSTATUS;

typedef struct /*_RCPT_ENTRY*/ {
	int dbid;
	RCPTSTATUS status;
} MAIL_RCPT;

typedef struct /*_OUTBOUND_MAIL*/ {
	int recipients;
	MAIL_RCPT* rcpt;
	char* envelopefrom;
	int length;
	char* data;
} MAIL;

typedef struct /*_DATABASE_CONN*/ {
	sqlite3* conn;
	sqlite3_stmt* query_outbound_hosts;
	sqlite3_stmt* query_remote;
	sqlite3_stmt* query_domain;
	sqlite3_stmt* query_rcpt;

	sqlite3_stmt* query_bounce_candidates;
	sqlite3_stmt* query_bounce_reasons;
	sqlite3_stmt* insert_bounce;
	sqlite3_stmt* insert_bounce_reason;
	sqlite3_stmt* delete_mail;
	sqlite3_stmt* update_failcount;
} DATABASE;

typedef struct /*_ARGUMENT_COLLECTION*/ {
	char* delivery_domain;
	DELIVERY_MODE delivery_mode;
	bool drop_privileges;
	bool daemonize;
	char* config_file;
} ARGUMENTS;

typedef struct /*_REMOTE_PORT*/ {
	uint16_t port;
	#ifndef CMAIL_NO_TLS
	TLSMODE tls_mode;
	#endif
} REMOTE_PORT;

typedef struct /*_MTA_SETTINGS*/ {
	char* helo_announce;
	REMOTE_PORT* port_list;
	unsigned check_interval;
	unsigned rate_limit;
	unsigned mail_retries;
	unsigned retry_interval;
	unsigned tls_padding;
	#ifndef CMAIL_NO_TLS
	gnutls_certificate_credentials_t tls_credentials;
	#endif
} MTA_SETTINGS;

typedef struct /*_CONFIGURATION_AGGREG*/ {
	DATABASE database;
	LOGGER log;
	USER_PRIVS privileges;
	MTA_SETTINGS settings;
} CONFIGURATION;

typedef struct /*_SMTP_REPLY*/ {
	unsigned code;
	bool multiline;
	char* response_text;
	unsigned buffer_length;
} SMTPREPLY;

typedef struct /*_SMTPCLIENT_CONN*/ {
	bool extensions_supported;
	time_t last_action;
	char recv_buffer[CMAIL_RECEIVE_BUFFER_LENGTH];
	ssize_t recv_offset;
	SMTPREPLY reply;
} CONNDATA;

//Prototypes where needed
int mail_dbread(LOGGER log, MAIL* mail, sqlite3_stmt* stmt);
int mail_dispatch(LOGGER log, DATABASE* database, MAIL* mail, CONNECTION* conn);
int mail_free(MAIL* mail);

#include "args.c"
#include "database.c"
#include "config.c"
#include "connection.c"
#include "protocol.c"
#include "smtp.c"
#include "logic.c"
#include "mail.c"
