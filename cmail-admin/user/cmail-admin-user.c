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

int generate_salt(char* out, unsigned chars) {
	const char* salt_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	uint8_t randomness[chars];
	int i;

	if(common_rand(randomness, chars) < 0){
		return -1;
	}

	for(i=0;i<chars;i++){
		out[i] = salt_alphabet[randomness[i]%strlen(salt_alphabet)];
	}

	return 0;
}

int set_password(LOGGER log, sqlite3* db, const char* user, char* password) {
	unsigned salt_len = 10;

	char salt[salt_len + 1];
	char password_hash[BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE) + 1];
	char auth_data[sizeof(salt) + sizeof(password_hash) + 1];

	memset(salt, 0, sizeof(salt));
	memset(password_hash, 0, sizeof(password_hash));
	memset(auth_data, 0, sizeof(auth_data));

	if(password){
		if(generate_salt(salt, salt_len) < 0){
			logprintf(log, LOG_ERROR, "Failed to generate a random salt\n");
			return 21;
		}

		logprintf(log, LOG_DEBUG, "Generated salt %s\n", salt);

		if(auth_hash(password_hash, sizeof(password_hash), salt, strlen(salt), password, strlen(password)) < 0) {
			logprintf(log, LOG_ERROR, "Error hashing password\n");
			return 21;
		}

		snprintf(auth_data, sizeof(auth_data), "%s:%s", salt, password_hash);
		return sqlite_set_password(log, db, user, auth_data);
	}
	else{
		return sqlite_set_password(log, db, user, NULL);
	}

}

int set_asked_password(LOGGER log, sqlite3* db, const char* user) {

	printf("Password: ");

	char pw[MAX_PW_LENGTH];
	memset(pw, 0, sizeof(pw));

	if (ask_password(pw, MAX_PW_LENGTH) < 0) {
		logprintf(log, LOG_ERROR, "Maximum password length is %d\n", MAX_PW_LENGTH);
		return 11;
	}

	printf("\n");
	return set_password(log, db, user, pw);
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
	printf("\t--password, -p\t\t ask for password in add\n");
	printf("commands:\n");
	printf("\tadd <username> [<pw>]\t\t adds an user. If -p is set then asked for password.\n");
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
	unsigned pw_flag = 0;

	char* dbpath = "/var/cmail/master.db3";
	srand(time(NULL));

	for (i = 1; i < argc; i++) {

		if (i + 1 < argc && (!strcmp(argv[i], "--dbpath") || !strcmp(argv[i], "-d"))) {
			dbpath = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			usage();
			return 0;
		} else if (!strcmp(argv[i], "--password") || !strcmp(argv[i], "-p")) {
			pw_flag = 1;
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
				status = set_asked_password(log, database.conn, argv[i + 2]);
			}

			sqlite3_close(database.conn);
			return status;

		} else if (i + 1 < argc && !strcmp(argv[i], "add")) {
			database.conn = database_open(log, dbpath, SQLITE_OPEN_READWRITE);

			if (!database.conn) {
				return 10;
			}

			status = add_user(log, database.conn, argv[i + 1]);

			if (status > 0) {
				sqlite3_close(database.conn);
				return status;
			}

			if (i + 2 < argc) {
				status = set_password(log, database.conn, argv[i + 1], argv[i + 2]);
			} else if (pw_flag) {
				status = set_asked_password(log, database.conn, argv[i + 1]);
			}

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
