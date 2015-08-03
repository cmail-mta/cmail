#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "../cmail-admin.h"

// logger support
#include "../../lib/logger.h"
#include "../../lib/logger.c"

// database functions
#include "../../lib/database.h"
#include "../../lib/database.c"

// common functions
#include "../../lib/common.h"
#include "../../lib/common.c"

// auth functions
#include "../../lib/auth.h"
#include "../../lib/auth.c"

#include "getpass.c"
#include "user.c"

#include "../lib/easy_args.h"

#define MAX_PW_LENGTH 256

// argument options
int verbosity = 0;
char* dbpath = DEFAULT_DBPATH;

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

int usage(char* fn){
	printf("cmail-admin-user: Part of the administration tool suite for cmail.\n");
	printf("Add, modify or delete cmail users.\n");
	printf("Usage: %s <options> <commands> <arguments>\n", fn);
	printf("Basic options:\n");
	printf("\t--verbosity, -v\t\t\tSet verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t\tSet master database path (Default: %s)\n", DEFAULT_DBPATH);
	printf("\t--help, -h\t\t\tDisplay this help message\n");
	printf("Commands:\n");
	printf("\tadd <username> [<password>]\tAdd an user\n");
	printf("\tpassword <user> [<pw>]\t\tUpdate a users password\n");
	printf("\trevoke <user>\t\t\tRevoke user login credentials (Sets NULL password)\n");
	printf("\tdelete <user>\t\t\tDelete an account\n");
	printf("\tlist [<filter>]\t\t\tList all active users, optionally filter by <filter>\n");
	return 1;
}

int mode_passwd(LOGGER log, sqlite3* db, int argc, char** argv){
	char password[MAX_PW_LENGTH];
	char* pw = password;
	
	memset(password, 0, sizeof(password));

	if(argc < 2){
		logprintf(log, LOG_ERROR, "Missing user name argument\n");
		return -1;
	}

	else if(argc == 2){
		fprintf(stderr, "Enter new password (leave blank to disable login): ");
		if(ask_password(password, MAX_PW_LENGTH) < 0){
			logprintf(log, LOG_ERROR, "Maximum password length is %d\n", MAX_PW_LENGTH);
			return 11;
		}
	}
	
	else{
		pw = argv[2];	
	}

	return set_password(log, db, argv[1], pw[0] ? pw:NULL);
}

int mode_add(LOGGER log, sqlite3* db, int argc, char** argv){
	if(argc < 2){
		logprintf(log, LOG_ERROR, "Missing user name argument\n\n");
		return -1;
	}
	else{
		if(sqlite_add_user(log, db, argv[1], NULL)){
			logprintf(log, LOG_ERROR, "Failed to create user\n");
			return -1;
		}
		return mode_passwd(log, db, argc, argv);
	}
}

int mode_revoke(LOGGER log, sqlite3* db, int argc, char** argv){
	if(argc != 2){
		logprintf(log, LOG_ERROR, "No user name supplied\n");
		return -1;
	}

	return set_password(log, db, argv[1], NULL);
}

int mode_delete(LOGGER log, sqlite3* db, int argc, char** argv){
	if(argc != 2){
		logprintf(log, LOG_ERROR, "No user name supplied\n");
		return -1;
	}
	return sqlite_delete_user(log, db, argv[1]);
}

int mode_list(LOGGER log, sqlite3* db, int argc, char** argv){
	char* filter = "%";
	
	if(argc >= 2){
		filter = argv[1];
	}

	return sqlite_get(log, db, filter);
}

int help(int argc, char* argv[]) {
	usage("cmail-admin-user");

	return -1;
}

int set_verbosity(int argc, char* argv[]) {

	verbosity = strtoul(argv[1], NULL, 10);

	return 0;
}

int set_dbpath(int argc, char* argv[]) {
	dbpath = argv[1];

	return 0;
}

int main(int argc, char* argv[]) {
	dbpath = getenv("CMAIL_MASTER_DB");
	
	if(!dbpath){
		dbpath = DEFAULT_DBPATH;
	}

	// argument parsing
	args_addArgument("-v", "--verbosity", set_verbosity, 1);
	args_addArgument("-h", "--help", help, 0);
	args_addArgument("-d", "--dbpath", set_dbpath, 1);

	char* cmds[argc * sizeof(char*)];
	int cmdsc = args_parse(argc, argv, cmds);

	LOGGER log = {
		.stream = stderr,
		.verbosity = verbosity
	};
	
	if (cmdsc < 0) {
		return 1;
	}

	int status = -1;

	sqlite3* db = NULL;

	if(!cmdsc){
		logprintf(log, LOG_ERROR, "No command specified\n\n");
		exit(usage(argv[0]));
	}

	logprintf(log, LOG_INFO, "Opening database at %s\n", dbpath);
	db = database_open(log, dbpath, SQLITE_OPEN_READWRITE);
	if(!db){
		//logprintf(log, LOG_ERROR, "Failed to open database at %s, please check the path\n\n", dbpath);
		exit(usage(argv[0]));
	}

	//select command
	if(!strcmp(cmds[0], "add")){
		status = mode_add(log, db, cmdsc - 1, cmds + 1);
	}
	else if(!strcmp(cmds[0], "delete")){
		status = mode_delete(log, db, cmdsc - 1, cmds + 1);
	}
	else if(!strcmp(cmds[0], "revoke")){
		status = mode_revoke(log, db, cmdsc - 1, cmds + 1);
	}
	else if(!strcmp(cmds[0], "list")){ 
		status = mode_list(log, db, cmdsc - 1, cmds + 1);
	}
	else if(!strcmp(cmds[0], "password") || !strcmp(cmds[0], "passwd")){
		status = mode_passwd(log, db, cmdsc - 1, cmds + 1);
	}
	else{
		logprintf(log, LOG_WARNING, "Unknown command %s\n\n", cmds[0]);
		usage(argv[0]);
	}

	sqlite3_close(db);
	return status;
}
