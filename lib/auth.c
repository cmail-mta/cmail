/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

int auth_hash(char* hash, unsigned hash_bytes, char* salt, unsigned salt_bytes, char* pass, unsigned pass_bytes){
	struct sha256_ctx hash_context;
	uint8_t digest[SHA256_DIGEST_SIZE];

	if(hash_bytes < BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE)){
		return -1;
	}

	sha256_init(&hash_context);

	sha256_update(&hash_context, salt_bytes, (uint8_t*)salt);
	sha256_update(&hash_context, pass_bytes, (uint8_t*)pass);

	sha256_digest(&hash_context, SHA256_DIGEST_SIZE, digest);
	base16_encode_update((uint8_t*)hash, SHA256_DIGEST_SIZE, digest);
	return BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE);
}

#ifdef CMAIL_HAVE_DATABASE_TYPE
int auth_validate(LOGGER log, DATABASE* database, char* user, char* password, char** authorized_identity){
	int status, rv = -1;
	char* user_salt;
	char* stored_hash;
	char digest_b16[BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE) + 1];
	char* auth_id = NULL;

	if(!user || !password){
		return -1;
	}

	memset(digest_b16, 0, sizeof(digest_b16));
	logprintf(log, LOG_DEBUG, "Trying to authenticate %s\n", user);

	if(sqlite3_bind_text(database->query_authdata, 1, user, -1, SQLITE_STATIC) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind auth data query parameter\n");
		sqlite3_reset(database->query_authdata);
		sqlite3_clear_bindings(database->query_authdata);
		return -1;
	}

	status = sqlite3_step(database->query_authdata);
	switch(status){
		case SQLITE_ROW:
			user_salt = (char*)sqlite3_column_text(database->query_authdata, 0);
			if(user_salt){
				stored_hash = index(user_salt, ':');
				if(!stored_hash){
					logprintf(log, LOG_INFO, "Rejecting authentication for user %s, database entry invalid\n", user);
					break;
				}

				//calculate credentials hash
				auth_hash(digest_b16, sizeof(digest_b16), user_salt, stored_hash - user_salt, password, strlen(password));

				if(!strcmp(stored_hash + 1, digest_b16)){
					auth_id = (char*)sqlite3_column_text(database->query_authdata, 1);
					logprintf(log, LOG_INFO, "Credentials for user %s OK, authorized identity: %s\n", user, auth_id ? auth_id:user);

					//handle aliasing
					if(authorized_identity){
						if(auth_id){
							*authorized_identity = common_strdup(auth_id);
						}
						else{
							*authorized_identity = common_strdup(user);
						}
					}
					rv = 0;
				}
				else{
					logprintf(log, LOG_INFO, "Credentials check failed for user %s: %s\n", user, digest_b16);
				}
			}
			else{
				logprintf(log, LOG_INFO, "Rejecting authentication for user %s, not enabled in database\n", user);
			}
			break;
		case SQLITE_DONE:
			logprintf(log, LOG_INFO, "Unknown user %s\n", user);
			break;
		default:
			logprintf(log, LOG_INFO, "Unhandled return value from auth data query: %d (%s)\n", status, sqlite3_errmsg(database->conn));
			break;
	}

	sqlite3_reset(database->query_authdata);
	sqlite3_clear_bindings(database->query_authdata);

	return rv;
}
#endif
