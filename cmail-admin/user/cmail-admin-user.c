#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#define CMAIL_ADMIN_AUTH
#include "../cmail-admin.h"

#include "getpass.c"
#include "user.c"

#define MAX_PW_LENGTH 256
#define PROGRAM_NAME "cmail-admin-user"

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

int set_password(sqlite3* db, const char* user, char* password) {
	unsigned salt_len = 10;

	char salt[salt_len + 1];
	char password_hash[BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE) + 1];
	char auth_data[sizeof(salt) + sizeof(password_hash) + 1];

	memset(salt, 0, sizeof(salt));
	memset(password_hash, 0, sizeof(password_hash));
	memset(auth_data, 0, sizeof(auth_data));

	if(password){
		if(generate_salt(salt, salt_len) < 0){
			logprintf(LOG_ERROR, "Failed to generate a random salt\n");
			return 21;
		}

		logprintf(LOG_DEBUG, "Generated salt %s\n", salt);

		if(auth_hash(password_hash, sizeof(password_hash), salt, strlen(salt), password, strlen(password)) < 0) {
			logprintf(LOG_ERROR, "Error hashing password\n");
			return 21;
		}

		snprintf(auth_data, sizeof(auth_data), "%s:%s", salt, password_hash);
		return sqlite_set_password(db, user, auth_data);
	}
	else{
		return sqlite_set_password(db, user, NULL);
	}

}

int usage(char* fn){
	printf("cmail-admin-users - Add, modify or delete cmail users\n");
	printf("This program is part of the cmail administration toolkit\n");
	printf("Usage: %s <options> <commands> <arguments>\n\n", fn);

	printf("Basic options:\n");
	printf("\t--verbosity, -v\t\t\tSet verbosity level (0 - 4)\n");
	printf("\t--dbpath, -d <dbpath>\t\tSet master database path (Default: %s)\n", DEFAULT_DBPATH);
	printf("\t--help, -h\t\t\tDisplay this help message\n\n");

	printf("Commands:\n");
	printf("\tlist [<filter>]\t\t\tList all active users, optionally filter by <filter>\n");
	printf("\tadd <username> [<password>]\tAdd an user\n");
	printf("\tpassword <user> [<pw>]\t\tUpdate a users password\n");
	printf("\talias <user> [<alias>]\t\tModify user to be an alias\n");
	printf("\tuserdb <user> [<path>]\t\tSet user database path\n");
	printf("\trevoke <user>\t\t\tRevoke user login credentials (Sets NULL password)\n");
	printf("\tdelete <user>\t\t\tDelete an account\n");
	return 1;
}

#include "../lib/common.c"

int mode_passwd(sqlite3* db, int argc, char** argv){
	char password[MAX_PW_LENGTH];
	char* pw = password;

	memset(password, 0, sizeof(password));

	if(argc < 2){
		logprintf(LOG_ERROR, "Missing user name argument\n");
		return -1;
	}

	else if(argc == 2){
		fprintf(stderr, "Enter new password (leave blank to disable login): ");
		if(ask_password(password, MAX_PW_LENGTH) < 0){
			logprintf(LOG_ERROR, "Maximum password length is %d\n", MAX_PW_LENGTH);
			return 11;
		}
	}

	else{
		pw = argv[2];
	}

	return set_password(db, argv[1], pw[0] ? pw:NULL);
}

int mode_add(sqlite3* db, int argc, char** argv){
	if(argc < 2){
		logprintf(LOG_ERROR, "Missing user name argument\n\n");
		return -1;
	}
	else{
		if(sqlite_add_user(db, argv[1], NULL)){
			logprintf(LOG_ERROR, "Failed to create user\n");
			return -1;
		}
		return mode_passwd(db, argc, argv);
	}
}

int mode_userdb(sqlite3* db, int argc, char** argv){
	if (argc < 2){
		logprintf(LOG_ERROR, "Missing arguments\n");
	}

	return sqlite_set_userdb(db, argv[1], (argc == 3 && strlen(argv[2]) > 1)?argv[2]:NULL);
}

int mode_alias(sqlite3* db, int argc, char** argv){
	if (argc < 2){
		logprintf(LOG_ERROR, "Missing arguments\n");
	}

	return sqlite_set_alias(db, argv[1], (argc == 3 && strcmp(argv[1], argv[2]))?argv[2]:NULL);
}

int mode_revoke(sqlite3* db, int argc, char** argv){
	if(argc != 2){
		logprintf(LOG_ERROR, "No user name supplied\n");
		return -1;
	}

	return set_password(db, argv[1], NULL);
}

int mode_delete(sqlite3* db, int argc, char** argv){
	if(argc != 2){
		logprintf(LOG_ERROR, "No user name supplied\n");
		return -1;
	}
	return sqlite_delete_user(db, argv[1]);
}

int mode_list(sqlite3* db, int argc, char** argv){
	char* filter = "%";

	if(argc >= 2){
		filter = argv[1];
	}

	return sqlite_get(db, filter);
}


int main(int argc, char* argv[]) {
	struct config config = {
		.verbosity = 0,
		.dbpath = getenv("CMAIL_MASTER_DB")
	};

	if(!config.dbpath){
		config.dbpath = DEFAULT_DBPATH;
	}

	// argument parsing
	add_args();

	char* cmds[argc];
	int cmdsc = eargs_parse(argc, argv, cmds, &config);

	log_verbosity(config.verbosity, false);

	if (cmdsc < 0) {
		return 1;
	}

	int status = -1;

	sqlite3* db = NULL;

	if(!cmdsc){
		logprintf(LOG_ERROR, "No command specified\n\n");
		exit(usage(argv[0]));
	}

	logprintf(LOG_INFO, "Opening database at %s\n", config.dbpath);
	db = database_open(config.dbpath, SQLITE_OPEN_READWRITE);
	if(!db){
		//logprintf(LOG_ERROR, "Failed to open database at %s, please check the path\n\n", dbpath);
		exit(usage(argv[0]));
	}

	//check database version
	if (database_schema_version(db) != CMAIL_CURRENT_SCHEMA_VERSION) {
		logprintf(LOG_ERROR, "The specified database (%s) is at an unsupported schema version.");
		sqlite3_close(db);
		return 11;
	}

	//select command
	if(!strcmp(cmds[0], "add")){
		status = mode_add(db, cmdsc, cmds);
	}
	else if(!strcmp(cmds[0], "delete")){
		status = mode_delete(db, cmdsc, cmds);
	}
	else if(!strcmp(cmds[0], "revoke")){
		status = mode_revoke(db, cmdsc, cmds);
	}
	else if(!strcmp(cmds[0], "list")){
		status = mode_list(db, cmdsc, cmds);
	}
	else if(!strcmp(cmds[0], "userdb")){
		status = mode_userdb(db, cmdsc, cmds);
	}
	else if(!strcmp(cmds[0], "alias")){
		status = mode_alias(db, cmdsc, cmds);
	}
	else if(!strcmp(cmds[0], "password") || !strcmp(cmds[0], "passwd")){
		status = mode_passwd(db, cmdsc, cmds);
	}
	else{
		logprintf(LOG_WARNING, "Unknown command %s\n\n", cmds[0]);
		usage(argv[0]);
	}

	sqlite3_close(db);
	return status;
}
