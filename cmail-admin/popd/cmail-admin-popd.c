#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"

#include "popd.c"

#define PROGRAM_NAME "cmail-admin-popd"

int usage(char* fn){
	printf("cmail-admin-popd - Manage cmail-popd ACL\n");
	printf("This program is part of the cmail administration toolkit\n");
	printf("Usage: %s <options> <commands> <arguments>\n\n", fn);

	printf("Basic options:\n");
	printf("\t--verbosity, -v\t\t\tSet verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t\tSet master database path (Default: %s)\n", DEFAULT_DBPATH);
	printf("\t--help, -h\t\t\tDisplay this help message\n");

	printf("Commands:\n");
	printf("\tenable <user>\t\tAdd the user to the ACL\n");
	printf("\tdisable <user>\t\tRemove the user from the ACL\n");
	printf("\tunlock <user>\t\tUnlock the maildrop, use in emergencies only!\n");
	printf("\tlist [<filter>]\t\tList all active users, optionally filter by <filter>\n");
	return 1;
}

#include "../lib/common.c"

int mode_add(LOGGER log, sqlite3* db, int argc, char* argv[]) {

	if (argc < 2) {
		logprintf(log, LOG_ERROR, "Missing user\n\n");
		return -1;
	}

	return sqlite_add_popd(log, db, argv[1]);

}

int mode_delete(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	if (argc < 2) {
		logprintf(log, LOG_ERROR, "Missing user\n\n");
		return -1;
	}

	return sqlite_delete_popd(log, db, argv[1]);
}

int mode_unlock(LOGGER log, sqlite3* db, int argc, char* argv[]) {

	if (argc < 2) {
		logprintf(log, LOG_ERROR, "Missing user\n\n");
		return -1;
	}

	return sqlite_update_popd(log, db, argv[1], 0);
}

int mode_list(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	char* filter = "%";

	if (argc > 1) {
		filter = argv[1];
	}

	return sqlite_get_popd(log, db, filter);
}

int main(int argc, char* argv[]) {
	struct config config = {
		.verbosity = 0,
		.dbpath = getenv("CMAIL_MASTER_DB")
	};


	if (!config.dbpath) {
		config.dbpath = DEFAULT_DBPATH;
	}

	add_args();

	char* cmds[argc];
	int cmdsc = eargs_parse(argc, argv, cmds, &config);

	LOGGER log = {
		.stream = stderr,
		.verbosity = config.verbosity
	};

	if (cmdsc < 0) {
		return 1;
	}

	sqlite3* db = NULL;

	if (!cmdsc) {
		logprintf(log, LOG_ERROR, "No command specified\n\n");
		exit(usage(argv[0]));
	}

	logprintf(log, LOG_INFO, "Opening database at %s\n", config.dbpath);
	db = database_open(log, config.dbpath, SQLITE_OPEN_READWRITE);

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
	}  else if (!strcmp(cmds[0], "unlock")) {
		status = mode_unlock(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "list")) {
		status = mode_list(log, db, cmdsc, cmds);
	} else {

		printf("Invalid or no arguments.\n");
		usage(argv[0]);
	}

	sqlite3_close(db);
	return status;
}
