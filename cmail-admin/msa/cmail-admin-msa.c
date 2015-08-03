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

#include "msa.c"

void usage() {


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
}

int main(int argc, char* argv[]) {

	LOGGER log = {
		.stream = stderr,
		.verbosity = 0
	};

	sqlite3* db = NULL;

	int i, status;

	char* dbpath = getenv("CMAIL_MASTER_DB");

	if (!dbpath) {
		dbpath = DEFAULT_DBPATH;
	}

	for (i = 1; i < argc; i++) {

		if (i + 1 < argc && (!strcmp(argv[i], "--dbpath") || !strcmp(argv[i], "-d"))) {
			dbpath = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage();
			return 0;
		} else if (i + 1 < argc && (!strcmp(argv[i], "--verbosity") || !strcmp(argv[i], "-v"))) {
			log.verbosity = strtoul(argv[i + 1], NULL, 10);
		} else if (i + 1 < argc && (!strcmp(argv[i], "add"))) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}

			int status = sqlite_add_msa_default(log, db, argv[i + 1]);

			sqlite3_close(db);
			return status;

		}  else if (i + 1 < argc && !strcmp(argv[i], "delete")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}

			status = sqlite_delete_msa(log, db, argv[i + 1]);
			sqlite3_close(db);
			return status;
		}  else if (i + 3 < argc && !strcmp(argv[i], "update")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}
			const char* arguments = NULL;

			if (i + 4 < argc) {
				arguments = argv[i+4];
			}

			status = sqlite_update_msa(log, db, argv[i + 1], argv[i + 2], argv[i + 3], arguments);
			sqlite3_close(db);
			return status;
		}  else if (!strcmp(argv[i], "list")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READONLY);

			if (!db) {
				return 10;
			}
			if (i + 1 < argc) {
				status = sqlite_get_msa(log, db, argv[i + 1]);
			} else {
				status = sqlite_get_all_msa(log, db);
			}
			sqlite3_close(db);
			return status;
		}
	}

	printf("Invalid or no arguments.\n");
	usage();

	return 0;
}
