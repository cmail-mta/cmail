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

#include <sqlite3.h>

#define VERSION 			"cmail 0.1"
#define MAX_CFGLINE 			2048
#define LISTEN_QUEUE_LENGTH 		128
#define MAX_SIMULTANEOUS_CLIENTS 	64
#define MAX_FQDN_LENGTH			300 //actually, 255

#include "smtplimits.h"

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
	STATE_RECIPIENTS,
	STATE_DATA
} SMTPSTATE;

typedef struct /*_CONNECTION*/ {
	int fd;
	void* aux_data;
} CONNECTION;

typedef struct /*_LISTEN_DATA*/ {
	char* announce_domain;
} LISTENER;

typedef struct /*_CLIENT_DATA*/ {
	CONNECTION* listener;
	SMTPSTATE state;
	char recv_buffer[SMTP_MAX_LINE_LENGTH];
	unsigned recv_offset;
	char peer_name[MAX_FQDN_LENGTH];
	MAIL current_mail;
	/*last_action*/
} CLIENT;

typedef struct /*_CONNECTION_AGGREGATE*/ {
	unsigned count;
	CONNECTION* conns;
} CONNPOOL;

typedef struct /*_PATH_POOL*/{
	unsigned count;
	MAILPATH** paths;
} PATHPOOL;

typedef struct /*_ARGS*/ {
	char* config_file;
	bool drop_privileges;
	bool detach;
} ARGUMENTS;

typedef struct /*_LOGGER*/ {
	FILE* stream;
	unsigned verbosity;
} LOGGER;

typedef struct /*_DATABASE_CONNECTION*/ {
	sqlite3* conn;
	sqlite3_stmt* query_addresses;
	sqlite3_stmt* query_inrouter;
	sqlite3_stmt* query_outrouter;
	struct {
		sqlite3_stmt** statements;
		char** users;
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
void logprintf(LOGGER log, unsigned level, char* fmt, ...);

#define LOG_ERROR 	0
#define LOG_WARNING 	0
#define LOG_INFO 	1
#define LOG_DEBUG 	3

#include "network.c"
#include "database.c"
#include "path.c"
#include "route.c"
#include "connpool.c"
#include "pathpool.c"
#include "mail.c"
#include "smtpstatemachine.c"
#include "client.c"
#include "coreloop.c"

#include "arguments.c"
#include "config.c"
