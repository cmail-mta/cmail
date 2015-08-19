#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"

#include "address.c"

#define PROGRAM_NAME "cmail-admin-address"

int usage(char* fn) {
	printf("%s: Administration tool for cmail.\n", PROGRAM_NAME);
	printf("usage:\n");
	printf("\t--verbosity, -v\t\t Set verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t path to master database\n");
	printf("\t--help, -h\t\t shows this help\n");
	printf("\tadd <expression> <username> [<order>] adds an address\n");
	printf("\tdelete <expression> \t deletes the given expression\n");
	printf("\tswitch <expression1> <expression2> switch order of the given expressions\n");
	printf("\tlist [<expression>] list all addresses or if defined only addresses like <expression>\n");

	return 1;
}

#include "../lib/common.c"


int mode_add(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	int status = 0;

	if (argc < 3) {
		logprintf(log, LOG_ERROR, "Missing arguments\n\n");
		return -1;
	}

	if (argc > 3) {
		status = sqlite_add_address_order(log, db, argv[1], argv[2], (unsigned) strtol(argv[3], NULL, 10));
	} else {
		status = sqlite_add_address(log, db, argv[1], argv[2]);
	}

	return status;
}

int mode_delete(LOGGER log, sqlite3* db, int argc, char* argv[]) {

	if (argc < 2) {
		logprintf(log, LOG_ERROR, "Missing user argument\n\n");
		return -1;
	}

	return sqlite_delete_address(log, db, argv[1]);
}

int mode_switch(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	if (argc < 3) {
		logprintf(log, LOG_ERROR,"Missing arguments\n\n");
		return -1;
	}
	
	return sqlite_switch(log, db, argv[1], argv[2]);

}

int mode_list(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	
	char* filter = "%";

	if (argc > 1) {
		filter = argv[1];
	}

	return sqlite_get_address(log, db, filter);
}

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

	LOGGER log = {
		.stream = stderr,
		.verbosity = config.verbosity
	};

	if (cmdsc < 0) {
		return 1;
	}

	sqlite3* db = NULL;
	
	logprintf(log, LOG_INFO, "Opening database at %s\n", config.dbpath);
	db = database_open(log, config.dbpath, SQLITE_OPEN_READWRITE);

	if (!db) {
		return 10;
	}

	int status;

	if (!strcmp(cmds[0], "add")) {
		status = mode_add(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "delete")) {
		status = mode_delete(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "switch")) {
		status = mode_switch(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "list")) {
		status = mode_list(log, db, cmdsc, cmds);
	} else {
		logprintf(log, LOG_ERROR, "Unkown command %s\n\n", cmds[0]);
		exit(usage(argv[0]));
	}

	return status;
}
