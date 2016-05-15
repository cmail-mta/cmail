#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"
#include "address.c"

#define PROGRAM_NAME "cmail-admin-address"

int usage(char* fn) {
	printf("cmail-admin-address - Configure inbound address routing\n");
	printf("This program is part of the cmail administration toolkit\n");
	printf("Usage: %s <options> <commands> <arguments>\n\n", fn);

	printf("Basic options:\n");
	printf("\t--verbosity, -v\t\tSet verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\tSet master database path (Default: %s)\n", DEFAULT_DBPATH);
	printf("\t--help, -h\t\tDisplay this help message\n");

	printf("Commands:\n");
	printf("\tadd <expression> <router> [<router argument> [<order>]]\t\tCreate an inbound path\n");
	printf("\tdelete <order>\t\t\t\t\t\t\tDelete a path expression\n");
	printf("\tupdate <order> <router> [<route argument>]\t\t\tUpdate routing information for a path\n");
	printf("\tswap <first> <second>\t\t\t\t\t\tSwap order of two expressions (given by their orders)\n");
	printf("\ttest <mailpath>\t\t\t\t\t\tTest <mailpath> against the address expression database\n");
	printf("\tlist [<expression>]\t\t\t\t\t\tList all currently configured paths, optionally filtered by <expression>\n");
	return 1;
}

#include "../lib/common.c"


int mode_add(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	int status = 0;

	if (argc < 3) {
		logprintf(log, LOG_ERROR, "Missing arguments\n\n");
		return -1;
	}

	if(!router_valid(argv[2])){
		logprintf(log, LOG_WARNING, "Unknown router %s, adding anyway\n", argv[2]);
	}

	if (argc == 3) {
		status = sqlite_add_address(log, db, argv[1], argv[2], NULL);
	} else if (argc == 4) {
		status = sqlite_add_address(log, db, argv[1], argv[2], argv[3]);
	}
	else if (argc == 5) {
		status = sqlite_add_address_order(log, db, argv[1], strtol(argv[4], NULL, 10), argv[2], argv[3]);
	}
	else{
		logprintf(log, LOG_ERROR, "Too many arguments\n\n");
		status = -1;
	}

	return status;
}

int mode_delete(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	if (argc < 2) {
		logprintf(log, LOG_ERROR, "Missing argument\n\n");
		return -1;
	}

	return sqlite_delete_address(log, db, strtol(argv[1], NULL, 10));
}

int mode_swap(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	if (argc < 3) {
		logprintf(log, LOG_ERROR, "Missing arguments\n\n");
		return -1;
	}

	return sqlite_swap(log, db, strtol(argv[1], NULL, 10), strtol(argv[2], NULL, 10));
}

int mode_update(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	if (argc < 3) {
		logprintf(log, LOG_ERROR, "Missing arguments\n\n");
		return -1;
	}

	if(!router_valid(argv[2])){
		logprintf(log, LOG_WARNING, "Unknown router %s, adding anyway\n", argv[2]);
	}

	if(argc == 3){
		return sqlite_update_address(log, db, strtol(argv[1], NULL, 10), argv[2], NULL);
	}
	else{
		return sqlite_update_address(log, db, strtol(argv[1], NULL, 10), argv[2], argv[3]);
	}
}

int mode_list(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	char* filter = "%";

	if (argc > 1) {
		filter = argv[1];
	}

	return sqlite_get_address(log, db, filter, false);
}

int mode_test(LOGGER log, sqlite3* db, int argc, char* argv[]) {
	char* filter = "%";

	if (argc > 1) {
		filter = argv[1];
	}

	return sqlite_get_address(log, db, filter, true);
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

	if(!cmdsc){
		logprintf(log, LOG_ERROR, "No command specified\n\n");
		exit(usage(argv[0]));
	}

	logprintf(log, LOG_INFO, "Opening database at %s\n", config.dbpath);
	db = database_open(log, config.dbpath, SQLITE_OPEN_READWRITE);

	if(!db){
		exit(usage(argv[0]));
	}

	//check database version
	if (database_schema_version(log, db) != CMAIL_CURRENT_SCHEMA_VERSION) {
		logprintf(log, LOG_ERROR, "The specified database (%s) is at an unsupported schema version.\n");
		sqlite3_close(db);
		return 11;
	}

	int status;

	if (!strcmp(cmds[0], "add")) {
		status = mode_add(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "delete")) {
		status = mode_delete(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "swap")) {
		status = mode_swap(log, db, cmdsc, cmds);
	} else if (!strcmp(cmds[0], "update")) {
		status = mode_update(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "list")) {
		status = mode_list(log, db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "test")) {
		status = mode_test(log, db, cmdsc, cmds);
	} else {
		logprintf(log, LOG_ERROR, "Unkown command %s\n\n", cmds[0]);
		exit(usage(argv[0]));
	}

	sqlite3_close(db);

	return status;
}
