int auth_base64decode(LOGGER log, char* in){
	uint32_t decode_buffer;
	int group, len, i;
	char* idx;

	char* base64_alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	len=strlen(in);

	if(len%4){
		logprintf(log, LOG_WARNING, "Input has invalid length for base64\n");
		return -1;
	}

	//decode to code point indices
	for(i=0;i<len;i++){
		if(in[i]=='='){
			in[i]=0;
			continue;
		}
		idx=index(base64_alphabet, in[i]);
		if(!idx){
			logprintf(log, LOG_WARNING, "Input string contains invalid characters\n");
			return -1;
		}
		in[i]=idx-base64_alphabet;
	}

	for(group=0;group<(len/4);group++){
		//stuff the buffer
		decode_buffer=0|(in[group*4]<<18);
		decode_buffer|=(in[(group*4)+1]<<12);
		decode_buffer|=(in[(group*4)+2]<<6);
		decode_buffer|=(in[(group*4)+3]);

		//read back decoded characters
		in[(group*3)]=(decode_buffer&0xFF0000)>>16;
		in[(group*3)+1]=(decode_buffer&0xFF00)>>8;
		in[(group*3)+2]=(decode_buffer&0xFF);
		in[(group*3)+3]=0;
	}

	return (group*3)+3;
}

int auth_hash(char* hash, unsigned hash_bytes, char* salt, unsigned salt_bytes, char* pass, unsigned pass_bytes){
	struct sha256_ctx hash_context;
	uint8_t digest[SHA256_DIGEST_SIZE];

	if(hash_bytes<BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE)){
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
int auth_validate(LOGGER log, DATABASE* database, char* user, char* password){
	int status, rv=-1;
	char* user_salt;
	char* stored_hash;
	char digest_b16[BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE)+1];

	if(!user || !password){
		return -1;
	}

	memset(digest_b16, 0, sizeof(digest_b16));
	logprintf(log, LOG_DEBUG, "Trying to authenticate %s\n", user);

	if(sqlite3_bind_text(database->query_authdata, 1, user, -1, SQLITE_STATIC)!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind auth data query parameter\n");
		sqlite3_reset(database->query_authdata);
		sqlite3_clear_bindings(database->query_authdata);
		return -1;
	}

	status=sqlite3_step(database->query_authdata);
	switch(status){
		case SQLITE_ROW:
			user_salt=(char*)sqlite3_column_text(database->query_authdata, 0);
			if(user_salt){
				stored_hash=index(user_salt, ':');
				if(!stored_hash){
					logprintf(log, LOG_INFO, "Rejecting authentication for user %s, database entry invalid\n", user);
					break;
				}

				//calculate credentials hash
				auth_hash(digest_b16, sizeof(digest_b16), user_salt, stored_hash-user_salt, password, strlen(password));

				if(!strcmp(stored_hash+1, digest_b16)){
					logprintf(log, LOG_INFO, "Credentials for user %s ok\n", user);
					rv=0;
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
