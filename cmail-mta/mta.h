#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <adns.h>
#include <time.h>

#include "../lib/common.h"
#include "smtplimits.h"
#include "../cmail-msa/smtplimits.h"

#define VERSION "cmail-mta 0.1"
#define CMAIL_MTA_INTERVAL 10

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

typedef enum /*_DELIVERY_MODE*/ {
	DELIVER_DOMAIN,
	DELIVER_HANDOFF
} DELIVERY_MODE;

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
	sqlite3_stmt* query_outbound_hosts;
	sqlite3_stmt* query_remote;
	sqlite3_stmt* query_domain;
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
} MTA_SETTINGS;

typedef struct /*_CONFIGURATION_AGGREG*/ {
	DATABASE database;
	LOGGER log;
	USER_PRIVS privileges;
	MTA_SETTINGS settings;
} CONFIGURATION;

typedef enum /*_SMTPCLIENT_STATE*/ {
	STATE_NEW,
	STATE_EHLO,
	STATE_HELO,
	STATE_IDLE,
	STATE_RECIPIENTS,
	STATE_DATA
} SMTPSTATE;

typedef struct /*_SMTPCLIENT_CONN*/ {
	SMTPSTATE state;
	time_t last_action;
	char recv_buffer[CMAIL_RECEIVE_BUFFER_LENGTH];
	ssize_t recv_offset;
} CONNDATA;

#include "args.c"
#include "database.c"
#include "config.c"
#include "mail.c"
#include "connection.c"
#include "protocol.c"
#include "logic.c"
