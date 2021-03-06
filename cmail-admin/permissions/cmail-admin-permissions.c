#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"
#include "permissions.c"
#define PROGRAM_NAME "cmail-admin-permissions"

int usage(char* fn){
	printf("cmail-admin-permissions - Manage permissions for the cmail web panel\n");
	printf("This program is part of the cmail administration toolkit\n");
	printf("Usage: %s <options> <commands> <arguments>\n\n", fn);

	printf("Basic options:\n");
	printf("\t--verbosity, -v\t\t\tSet verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t\tSet master database path (Default: %s)\n", DEFAULT_DBPATH);
	printf("\t--help, -h\t\t\tDisplay this help message\n");

	printf("Commands:\n");
	printf("\tgrant <user> <permissions>\t\t\tGrant permissions to a user\n");
	printf("\trevoke <user> [<permission> [, <permission>]] \trevokes the given permission or all permissions if no permission is given from the user\n");
	printf("\tlist [<user>]\t\t\t\t list all permissions or permissions of the given user (or like the user expression)\n");
	printf("\trlist <permission>\t\t\t\t list all users like the given permission\n");
	printf("\tdelete user <delete>\t\t\t\t delete the user delegation from user\n");
	printf("\tdelete expression <delete>\t\t\t delete the address space delegation from user\n");
	printf("\tdelegate user <user> <delegated>\t delegates a user to the given <user>\n");
	printf("\tdelegate address <user> <expression>\t delegates the given address space to the user\n");
	return 1;
}

#include "../lib/common.c"

int mode_grant(sqlite3* db, int argc, char** argv) {

	if (argc < 3) {
		logprintf(LOG_ERROR, "Not enough arguments for grant\n\n");
		return 20;
	}

	return sqlite_add_permission(db, argv[1], argv[2]);
}

int mode_revoke(sqlite3* db, int argc, char** argv) {
	int status = 0;

	if (argc < 2) {
		logprintf(LOG_ERROR, "Not enough arguments for revoke\n\n");
		return 20;
	} else if (argc > 2) {
		int i;
		// for every permission
		for (i = 2; i < argc; i++) {
			if (!status) {
				status = sqlite_delete_permission(db, argv[1], argv[i]);
			}
		}
	} else {
		status = sqlite_delete_permissions(db, argv[1]);
	}
	return status;
}

int mode_delete(sqlite3* db, int argc, char** argv) {
	int status = 0;

	if (argc < 4) {
		logprintf(LOG_ERROR, "Not enough arguments for delete\n\n");
		return 20;
	}

	if (!strcmp(argv[1], "expression")) {
		status = sqlite_delete_address_delegation(db, argv[2], argv[3]);
	} else if (!strcmp(argv[1], "user")) {
		status = sqlite_delete_user_delegation(db, argv[2], argv[3]);
	} else {
		logprintf(LOG_ERROR, "Mode %s not recognised\n\n", argv[1]);
		return 120;
	}
	return status;
}

int mode_delegate(sqlite3* db, int argc, char** argv) {
	int status = 0;

	if (argc < 2) {
		status = sqlite_get_delegated(db, "%");
	// add delegated address
	} else if (!strcmp(argv[1], "address")) {

		// list delegated addresses
		if (argc < 3) {
			status = sqlite_get_delegated_addresses(db, "%");
		// list delegated addresses by user
		} else if (argc < 4) {
			status = sqlite_get_delegated_addresses(db, argv[2]);
		// add delegated addresses
		} else {
			status = sqlite_delegate_address(db, argv[2], argv[3]);
		}
	// add delegated user
	} else if (!strcmp(argv[1], "user")) {

		// list all delegated users
		if (argc < 3) {
			status = sqlite_get_delegated_users(db, "%");
		// list delegated users by username
		} else if (argc < 4) {
			status = sqlite_get_delegated_users(db, argv[2]);
		// add delegation
		} else {
			status = sqlite_delegate_user(db, argv[2], argv[3]);
		}
	} else {
		status = sqlite_get_delegated(db, argv[1]);
	}
	return status;
}

int mode_list(sqlite3* db, int argc, char** argv) {

	char* filter = "%";
	if (argc > 1) {
		filter = argv[1];
	}

	return sqlite_get_permissions(db, filter);
}

int mode_rlist(sqlite3* db, int argc, char** argv) {

	if (argc < 2) {
		logprintf(LOG_ERROR, "Permission to list is missing\n\n");
		return 20;
	}

	return sqlite_get_permissions_by_permission(db, argv[1]);
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

	log_verbosity(config.verbosity, false);

	if (cmdsc < 0) {
		return 1;
	}

	sqlite3* db = NULL;
	int status = 0;


	if (!cmdsc) {
		logprintf(LOG_ERROR, "No command specified\n\n");
		return usage(argv[0]);
	}

	logprintf(LOG_INFO, "Opening database at %s\n", config.dbpath);
	db = database_open(config.dbpath, SQLITE_OPEN_READWRITE);

	if (!db) {
		usage(argv[0]);
		return 10;
	}

	//check database version
	if (database_schema_version(db) != CMAIL_CURRENT_SCHEMA_VERSION) {
		logprintf(LOG_ERROR, "The specified database (%s) is at an unsupported schema version.");
		sqlite3_close(db);
		return 11;
	}

	if (!strcmp(cmds[0], "grant")) {
		status = mode_grant(db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "revoke")) {
		status = mode_revoke(db, cmdsc, cmds);
	} else if (!strcmp(cmds[0], "delete")) {
		status = mode_delete(db, cmdsc, cmds);
	} else if (!strcmp(cmds[0], "delegate")) {
		status = mode_delegate(db, cmdsc, cmds);
	}  else if (!strcmp(cmds[0], "list")) {
		status = mode_list(db, cmdsc, cmds);
	} else if (!strcmp(cmds[0], "rlist")) {
		status = mode_rlist(db, cmdsc, cmds);
	} else {
		logprintf(LOG_WARNING, "Unkown command %s\n\n", cmds[0]);
		usage(argv[0]);
	}

	sqlite3_close(db);
	return status;
}
