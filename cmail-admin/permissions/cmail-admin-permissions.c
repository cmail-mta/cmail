#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"

// logger support
#include "../../lib/logger.h"
#include "../../lib/logger.c"

// database suff
#include "../../lib/database.h"
#include "../../lib/database.c"

// common stuff
#include "../../lib/common.h"
#include "../../lib/common.c"

#include "permissions.c"

void usage() {


	printf("cmail-admin-permissions: Administration tool for cmail-rights.\n");
	printf("usage:\n");
	printf("\n");
	printf("options:\n");
	printf("\t--verbosity, -v\t\t\t\t Set verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t\t\t path to master database\n");
	printf("\t--help, -h\t\t\t\t shows this help\n");
	printf("commands:\n");
	printf("\tgrant <user> <permissions>\t\t\t adds the given permission to the user\n");
	printf("\trevoke <user> [<permissions>] \t\t revokes the given permission or all permissions if no permission is given from the user\n");
	printf("\tlist [<user>]\t\t\t\t list all permissions or permissions of the given user (or like the user expression)\n");
	printf("\trlist <right>\t\t\t\t list all users like the given permission\n");
	printf("\tdelete user <delete>\t\t\t\t delete the user delegation from user\n");
	printf("\tdelete expression <delete>\t\t\t delete the address space delegation from user\n");
	printf("\tdelegate user <user> <delegated>\t delegates a user to the given <user>\n");
	printf("\tdelegate address <user> <expression>\t delegates the given address space to the user\n");
}

int main(int argc, char* argv[]) {

	LOGGER log = {
		.stream = stderr,
		.verbosity = 0
	};

	sqlite3* db = NULL;
	int i, status;

	char* dbpath = "/var/cmail/master.db3";

	for (i = 1; i < argc; i++) {

		if (i + 1 < argc && (!strcmp(argv[i], "--dbpath") || !strcmp(argv[i], "-d"))) {
			dbpath = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage();
			return 0;
		} else if (i + 1 < argc && (!strcmp(argv[i], "--verbosity") || !strcmp(argv[i], "-v"))) {
			log.verbosity = strtoul(argv[i + 1], NULL, 10);
		} else if (i + 2 < argc && (!strcmp(argv[i], "grant"))) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}

			int status = sqlite_add_right(log, db, argv[i + 1], argv[i + 2]);

			sqlite3_close(db);
			return status;

		}  else if (i + 1 < argc && !strcmp(argv[i], "revoke")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}

			if (i + 2 < argc) {
				status = sqlite_delete_right(log, db, argv[i + 1], argv[i + 2]);
			} else {
				status = sqlite_delete_rights(log, db, argv[i + 1]);
			}
			sqlite3_close(db);
			return status;
		} else if (i + 3 < argc &&!strcmp(argv[i], "delete")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}

			if (!strcmp(argv[i + 1], "expression")) {
				status = sqlite_delete_address_delegation(log, db, argv[i + 2], argv[i + 3]);
			} else if (!strcmp(argv[i + 1], "user")) {
				status = sqlite_delete_user_delegation(log, db, argv[i + 2], argv[i + 3]);
			} else {
				return 120;
			}

			sqlite3_close(db);
			return status;


		} else if (!strcmp(argv[i], "delegate")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}

			if (i + 3 < argc && !strcmp(argv[i + 1], "address")) {
				status = sqlite_delegate_address(log, db, argv[i + 2], argv[i + 3]);
			} else if (i + 3 < argc && !strcmp(argv[i + 1], "user")) {
				status = sqlite_delegate_user(log, db, argv[i + 2], argv[i + 3]);
			} else {
				if (i + 1 < argc) {
					status = sqlite_get_delegated(log, db, argv[i + 1]);
				} else {
					status = sqlite_get_delegated_all(log, db);
				}
			}

			sqlite3_close(db);
			return status;
		}  else if (!strcmp(argv[i], "list")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READONLY);

			if (!db) {
				return 10;
			}
			if (i + 1 < argc) {
				status = sqlite_get_rights(log, db, argv[i + 1]);
			} else {
				status = sqlite_get_all_rights(log, db);
			}
			sqlite3_close(db);
			return status;
		} else if (i + 1 < argc &&!strcmp(argv[i], "rlist")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READONLY);

			if (!db) {
				return 10;
			}

			status = sqlite_get_rights_by_right(log, db, argv[i + 1]);
			sqlite3_close(db);
			return status;
		}
	}

	printf("Invalid or no arguments.\n");
	usage();

	return 0;
}
