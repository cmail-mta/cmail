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

#include "address.c"

void usage() {


	printf("cmail-admin-address: Administration tool for cmail-mta.\n");
	printf("usage:\n");
	printf("\t--verbosity, -v\t\t Set verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t path to master database\n");
	printf("\t--help, -h\t\t shows this help\n");
	printf("\tadd <expression> <username> [<order>] adds an address\n");
	printf("\tdelete <expression> \t deletes the given expression\n");
	printf("\tswitch <expression1> <expression2> switch order of the given expressions\n");
	printf("\tlist [<expression>] list all addresses or if defined only addresses like <expression>\n");
}

int main(int argc, char* argv[]) {

	LOGGER log = {
		.stream = stderr,
		.verbosity = 0
	};

	DATABASE database = {
		.conn = NULL
	};

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
		} else if (i + 2 < argc && (!strcmp(argv[i], "add"))) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!database.conn) {
				return 10;
			}

			int status;
			if (i + 3 < argc) {
				status = sqlite_add_address_order(log, database.conn, argv[i + 1], argv[i + 2], (unsigned) strtol(argv[i + 3], NULL, 10));
			} else {
				status = sqlite_add_address(log, database.conn, argv[i + 1], argv[i + 2]);
			}
			sqlite3_close(database.conn);
			return status;

		}  else if (i + 1 < argc && !strcmp(argv[i], "delete")) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!database.conn) {
				return 10;
			}

			status = sqlite_delete_address(log, database.conn, argv[i + 1]);
			sqlite3_close(database.conn);
			return status;
		}  else if (i + 2 < argc && !strcmp(argv[i], "switch")) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!database.conn) {
				return 10;
			}

			status = sqlite_switch(log, database.conn, argv[i + 1], argv[i + 2]);
			sqlite3_close(database.conn);
			return status;
		}  else if (!strcmp(argv[i], "list")) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READONLY);

			if (!database.conn) {
				return 10;
			}
			if (i + 1 < argc) {
				status = sqlite_get_address(log, database.conn, argv[i + 1]);
			} else {
				status = sqlite_get_all_addresses(log, database.conn);
			}
			sqlite3_close(database.conn);
			return status;
		}
	}

	printf("Invalid or no arguments.\n");
	usage();

	return 0;
}
