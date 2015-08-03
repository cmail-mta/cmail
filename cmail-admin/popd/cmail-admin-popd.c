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

#include "popd.c"

void usage() {


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

			int status = sqlite_add_popd(log, db, argv[i + 1]);

			sqlite3_close(db);
			return status;

		}  else if (i + 1 < argc && !strcmp(argv[i], "delete")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}

			status = sqlite_delete_popd(log, db, argv[i + 1]);
			sqlite3_close(db);
			return status;
		}  else if (i + 1 < argc && !strcmp(argv[i], "unlock")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!db) {
				return 10;
			}

			status = sqlite_update_popd(log, db, argv[i + 1], 0);
			sqlite3_close(db);
			return status;
		}  else if (!strcmp(argv[i], "list")) {
			db = database_open(log, dbpath, SQLITE_OPEN_READONLY);

			if (!db) {
				return 10;
			}
			if (i + 1 < argc) {
				status = sqlite_get_popd(log, db, argv[i + 1]);
			} else {
				status = sqlite_get_popd(log, db, "%");
			}
			sqlite3_close(db);
			return status;
		}
	}

	printf("Invalid or no arguments.\n");
	usage();

	return 0;
}
