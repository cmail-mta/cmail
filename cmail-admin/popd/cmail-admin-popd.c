#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"

#include "popd.c"

#define PROGRAM_NAME "cmail-admin-popd"

int usage(char* fn) {


	printf("cmail-admin-popd: Administration tool for cmail-popd.\n");
	printf("usage:\n");
	printf("\t--verbosity, -v\t\t Set verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t path to master database\n");
	printf("\t--help, -h\t\t shows this help\n");
	printf("\tadd <user>\t\t enables <user> for pop\n");
	printf("\tdelete <user> \t disables <user> for pop\n");
	printf("\tunlock <user> \t  unlocks the given user.\n");
	printf("\tlist [<user>] list all popd entries or if defined only popd entries like <user>\n");
	printf("\n");

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
	char* dbpath = getenv("CMAIL_MASTER_DB");

	if (!dbpath) {
		dbpath = DEFAULT_DBPATH;
	}

	add_args();

	char* cmds[argc * sizeof(char*)];
	int cmdsc = eargs_parse(argc, argv, cmds);

	LOGGER log = {
		.stream = stderr,
		.verbosity = verbosity
	};

	if (cmdsc < 0) {
		return 1;
	}

	sqlite3* db = NULL;

	if (!cmdsc) {
		logprintf(log, LOG_ERROR, "No command specified\n\n");
		exit(usage(argv[0]));
	}

	logprintf(log, LOG_INFO, "Opening database at %s\n", dbpath);
	db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

	if (!db) {
		exit(usage(argv[0]));
	}

	int status = 20;

	if (!strcmp(cmds[0], "add")) {
		status = mode_add(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "delete")) {
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
