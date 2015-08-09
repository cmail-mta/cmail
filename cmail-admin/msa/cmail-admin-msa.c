#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"

#include "msa.c"

#define PROGRAM_NAME "cmail-admin-msa"

int usage(char* fn) {

	printf("cmail-admin-msa: Administration tool for cmail-msa.\n");
	printf("usage:\n");
	printf("\t--verbosity, -v\t\t Set verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t path to master database\n");
	printf("\t--help, -h\t\t shows this help\n");
	printf("\tadd <user>\t\t adds the default config to the user\n");
	printf("\tdelete <user> \t deletes the given user msa entry\n");
	printf("\tupdate <router> <user> <rtype> [<arg>] update the given router from given user.\n");
	printf("\tlist [<user>] list all msa entries or if defined only msa entries like <expression>\n");
	printf("\n");
	printf("Available router:\n");
	printf("\toutrouter\trouting options for outgoing\n");
	printf("\tinrouter\trouting options for incoming\n");
	printf("For more see documentation.\n");

	return 10;
}

int mode_add(LOGGER log, sqlite3* db, int argc, char** argv) {

	if (argc != 2) {
		logprintf(log, LOG_ERROR, "No user name supplied\n");
		return -1;
	}
	return sqlite_add_msa_default(log, db, argv[1]);
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
	
	const char* arguments = NULL;
	if (argc < 2 ) {
		logprintf(log, LOG_ERROR, "No router supplied\n");
		return -1;
	} else if (argc < 3 ) {
		logprintf(log, LOG_ERROR, "No user supplied\n");
		return -2;
	} else if (argc < 4) {
		logprintf(log, LOG_ERROR, "No router type supplied\n");
		return -3;
	} else {
		arguments = argv[4];
	}

	return sqlite_update_msa(log, db, argv[1], argv[2], argv[3], arguments);
}

// common stuff for parsing (needs usage method above)
#include "../lib/common.c"

int main(int argc, char* argv[]) {
	char* dbpath = getenv("CMAIL_MASTER_DB");

	if (!dbpath) {
		dbpath = DEFAULT_DBPATH;
	}

	// argument parsing
	add_args();

	char* cmds[argc * sizeof(char*)];
	int cmdsc = eargs_parse(argc, argv, cmds);

	// check for errors in parsing
	if (cmdsc < 0) {
		return 1;
	}

	LOGGER log = {
		.stream = stderr,
		.verbosity = verbosity
	};

	sqlite3* db = NULL;

	// check for commands
	if (!cmdsc) {
		logprintf(log, LOG_ERROR, "No command specified\n\n");
		exit(usage(argv[0]));
	}

	logprintf(log, LOG_INFO, "Opening database at %s\n", dbpath);
	db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

	// check for database opening errors
	if (!db) {
		exit(usage(argv[0]));
	}

	int status = 20;

	if (!strcmp(cmds[0], "add")) {
		status = mode_add(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "delete")) {
		status = mode_list(log, db, cmdsc, cmds);
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
