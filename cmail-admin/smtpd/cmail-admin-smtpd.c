#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"
#include "smtpd.c"
#define PROGRAM_NAME "cmail-admin-smtpd"

int usage(char* fn) {
	printf("cmail-admin-smtpd - Manage cmail-smtpd ACL\n");
	printf("This program is part of the cmail administration toolkit\n");
	printf("Usage: %s <options> <commands> <arguments>\n\n", fn);

	printf("Basic options:\n");
	printf("\t--verbosity, -v\t\tSet verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\tSet master database path (Default: %s)\n", DEFAULT_DBPATH);
	printf("\t--help, -h\t\tDisplay this help message\n");

	printf("Commands:\n");
	printf("\tenable <user> [<router> [<router-argument>]]\tAdd <user> to smtpd ACL\n");
	printf("\tdisable <user>\t\t\t\t\tDelete <user> from smtpd ACL\n");
	printf("\tupdate <user> <router> [<router-argument>]\tUpdate the origination router for <user>\n");
	printf("\tlist [<user>]\t\t\t\t\tList currently active entries, optionally filtered by <user>\n");

	return 10;
}

int mode_add(LOGGER log, sqlite3* db, int argc, char** argv) {
	if (argc < 2) {
		logprintf(log, LOG_ERROR, "Missing arguments\n");
		return -1;
	}

	return sqlite_add_msa(log, db, argv[1], (argc > 2) ? argv[2]:NULL, (argc > 3) ? argv[3]:NULL);
}

int mode_delete(LOGGER log, sqlite3* db, int argc, char** argv) {
	if (argc != 2) {
		logprintf(log, LOG_ERROR, "No user name supplied\n");
		return -1;
	}

	return sqlite_delete_msa(log, db, argv[1]);
}

int mode_list(LOGGER log, sqlite3* db, int argc, char** argv) {
	char* filter = "%";

	if (argc >= 2) {
		filter = argv[1];
	}

	return sqlite_get_msa(log, db, filter);
}

int mode_update(LOGGER log, sqlite3* db, int argc, char** argv) {
	if (argc < 3 ) {
		logprintf(log, LOG_ERROR, "Missing arguments\n");
		return -1;
	}

	return sqlite_update_msa(log, db, argv[1], argv[2], (argc > 3) ? argv[3]:NULL);
}

// common stuff for parsing (needs usage method above)
#include "../lib/common.c"

int main(int argc, char* argv[]) {
	struct config config = {
		.verbosity = 0,
		.dbpath = getenv("CMAIL_MASTER_DB")
	};

	if (!config.dbpath) {
		config.dbpath = DEFAULT_DBPATH;
	}

	// argument parsing
	add_args();

	char* cmds[argc];
	int cmdsc = eargs_parse(argc, argv, cmds, &config);

	// check for errors in parsing
	if (cmdsc < 0) {
		return 1;
	}

	LOGGER log = {
		.stream = stderr,
		.verbosity = config.verbosity
	};

	sqlite3* db = NULL;

	// check for commands
	if (!cmdsc) {
		logprintf(log, LOG_ERROR, "No command specified\n\n");
		exit(usage(argv[0]));
	}

	logprintf(log, LOG_INFO, "Opening database at %s\n", config.dbpath);
	db = database_open(log, config.dbpath, SQLITE_OPEN_READWRITE);

	// check for database opening errors
	if (!db) {
		exit(usage(argv[0]));
	}

	//check database version
	if (database_schema_version(log, db) != CMAIL_CURRENT_SCHEMA_VERSION) {
		logprintf(log, LOG_ERROR, "The specified database (%s) is at an unsupported schema version.");
		sqlite3_close(db);
		return 11;
	}

	int status = 20;

	if (!strcmp(cmds[0], "enable")) {
		status = mode_add(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "disable")) {
		status = mode_delete(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "update")) {
		status = mode_update(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "list")) {
		status = mode_list(log, db, cmdsc, cmds);
	} else {

		logprintf(log, LOG_WARNING, "Unkown command %s\n\n", cmds[0]);
		usage(argv[0]);
	}

	sqlite3_close(db);

	return status;
}
