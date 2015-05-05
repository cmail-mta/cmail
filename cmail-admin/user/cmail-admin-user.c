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

// for auth stuff
#include "../../lib/auth.h"
#include "../../lib/auth.c"

#include "getpass.c"
#include "user.c"

#define MAX_PW_LENGTH 256

void gen_salt(char* salt, unsigned n) {

	unsigned i, j, counter = 0;
	int salt_raw;
	uint8_t salt_arr[n];
	
	for (j = 0; j < (n / 4); j++) {
		salt_raw = rand();
		for (i = 0; i < 4; i++) {
			salt_arr[counter] = (salt_raw & (0xFF << 8 * i)) >> 8 * i;
			counter++;
		}
	}

	salt_raw = rand();
	for (j = 0; j < (n % 4); j++) {
		salt_arr[counter] = (salt_raw & (0xFF << 8 * j)) >> 8 * j;
		counter++;
	}
	base16_encode_update((uint8_t*) salt, n, salt_arr);
}

int set_password(LOGGER log, sqlite3* db, const char* user, char* password) {

	unsigned n = 4;
	char salt[BASE16_ENCODE_LENGTH(n) +1];
	char hashed[BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE) +1];
	char auth[sizeof(salt) + sizeof(hashed) +1];

	memset(salt, 0, sizeof(salt));
	memset(auth, 0, sizeof(auth));

	if (password) {
		gen_salt(salt, n);
		if (auth_hash(hashed, sizeof(hashed), salt, strlen(salt), password, strlen(password)) < 0) {
			logprintf(log, LOG_ERROR, "Error hashing password\n");
			return 21;
		}
		snprintf(auth, sizeof(auth), "%s:%s", salt, hashed);
		return sqlite_set_password(log, db, user, auth);
	} else {
		return sqlite_set_password(log, db, user, NULL);
	}
		
}

int add_user(LOGGER log, sqlite3* db, const char* user) {

	return sqlite_add_user(log, db, user, NULL);
}

void usage() {


	printf("cmail-admin-user: Administration tool for cmail-mta.\n");
	printf("usage: cmail-admin-user <options> <commands> <arguments>\n");
	printf("\n");
	printf("basic options:\n");
	printf("\t--verbosity, -v\t\t\t Set verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t\t path to master database\n");
	printf("\t--help, -h\t\t\t shows this help\n");
	printf("commands:\n");
	printf("\tadd <username>\t\t\t adds an user, (password is set to null)\n");
	printf("\tset password <user> [<pw>]\t sets the password of the given user (if not given, ask for password\n");
	printf("\trevoke <user>\t\t\t revokes the access to the given user (sets password to null)\n");
	printf("\tdelete <user>\t\t\t deletes the given user\n");
	printf("\tlist [<username>]\t\t list all users or if defined only users like <username>\n");
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
	srand(time(NULL));

	for (i = 1; i < argc; i++) {

		if (i + 1 < argc && (!strcmp(argv[i], "--dbpath") || !strcmp(argv[i], "-d"))) {
			dbpath = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage();
			return 0;
		} else if (i + 1 < argc && (!strcmp(argv[i], "--verbosity") || !strcmp(argv[i], "-v"))) {
			log.verbosity = strtoul(argv[i + 1], NULL, 10);
		} else if (i + 2 < argc && (!strcmp(argv[i], "set") && !strcmp(argv[i + 1], "password"))) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!database.conn) {
				return 10;
			}

			int status;
			if (i + 3 < argc) {
				status = set_password(log, database.conn, argv[i + 2], argv[i + 3]);
			} else {
				char pw[MAX_PW_LENGTH];
				if (ask_password(pw, MAX_PW_LENGTH) < 0) {
					logprintf(log, LOG_ERROR, "Maximal password size is %d\n", MAX_PW_LENGTH);
					return 11;
				}

				status = set_password(log, database.conn, argv[i + 2], pw);
			}

			sqlite3_close(database.conn);
			return status;

		} else if (i + 1 < argc && !strcmp(argv[i], "add")) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!database.conn) {
				return 10;
			}

			status = add_user(log, database.conn, argv[i + 1]);

			sqlite3_close(database.conn);
			return status;
		} else if (i + 1 < argc && !strcmp(argv[i], "delete")) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!database.conn) {
				return 10;
			}

			status = sqlite_delete_user(log, database.conn, argv[i + 1]);
			sqlite3_close(database.conn);
			return status;
		} else if (i +1 < argc && !strcmp(argv[i], "revoke")) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!database.conn) {
				return 10;
			}

			status = set_password(log, database.conn, argv[i + 1], NULL);
			return status;	
		
		} else if (!strcmp(argv[i], "list")) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READONLY);

			if (!database.conn) {
				return 10;
			}
			if (i + 1 < argc) {
				status = sqlite_get(log, database.conn, argv[i + 1]);
			} else {
				status = sqlite_get_all(log, database.conn);
			}
			sqlite3_close(database.conn);
			return status;
		}
	}

	printf("Invalid or no arguments.\n");
	usage();

	return 0;
}
