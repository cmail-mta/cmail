int auth_reset(AUTH_DATA* auth_data){
	AUTH_DATA empty = {
		.method = AUTH_PLAIN,
		.user = NULL,

		.parameter = NULL,
		.challenge = NULL,
		.response = NULL
	};

	if(auth_data->user){
		free(auth_data->user);
	}

	if(auth_data->parameter){
		free(auth_data->parameter);
	}

	if(auth_data->challenge){
		free(auth_data->challenge);
	}

	if(auth_data->response){
		free(auth_data->response);
	}

	*auth_data=empty;
	return 0;
}

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

int auth_validate(LOGGER log, DATABASE* database, char* user, char* password){
	int status, rv=-1;
	char* user_salt;
	char* stored_hash;
	uint8_t digest[SHA256_DIGEST_SIZE];
	char digest_b16[BASE16_ENCODE_LENGTH(SHA256_DIGEST_SIZE)+1];
	struct sha256_ctx hash_context;
	
	if(!user || !password){
		return -1;
	}

	memset(digest_b16, 0, sizeof(digest_b16));
	logprintf(log, LOG_DEBUG, "Trying to authenticate %s with password %s\n", user, password);

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
				sha256_init(&hash_context);
				sha256_update(&hash_context, stored_hash-user_salt, (uint8_t*)user_salt);
				sha256_update(&hash_context, strlen(password), (uint8_t*)password);
				sha256_digest(&hash_context, SHA256_DIGEST_SIZE, digest);
				base16_encode_update((uint8_t*)digest_b16, SHA256_DIGEST_SIZE, digest);

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

int auth_method_plain(LOGGER log, DATABASE* database, AUTH_DATA* auth_data){
	int length, i, rv;
	char* user=NULL;
	char* pass=NULL;
	
	if(!auth_data->parameter){
		return 1;
	}

	//FIXME might use libnettle here
	length=auth_base64decode(log, auth_data->parameter);

	if(length<4){ //2x NUL, 2x min 1 byte
		logprintf(log, LOG_ERROR, "Failed to decode PLAIN authentication parameter\n");
		return -1;
	}

	//RFC 4616 message = [authzid] UTF8NUL authcid UTF8NUL passwd
	//skip authorization parameter
	//FIXME this might be vulnerable
	for(i=0;i<length;i++){
		if(auth_data->parameter[i]==0){
			if(!user){
				//Heap-copy the user to be able to properly interact with auth_free
				user=common_strdup(auth_data->parameter+i+1);
				if(!user){
					logprintf(log, LOG_ERROR, "Failed to allocate memory for user name copy\n");
					return -1;
				}
			}
			else if(!pass){
				pass=auth_data->parameter+i+1;
			}
		}
	}

	rv=auth_validate(log, database, user, pass);
	if(rv==0){
		//clean auth data
		auth_reset(auth_data);
		//authenticate the user
		auth_data->user=user;
	}
	else{
		free(user);
	}

	return rv;
}

int auth_status(LOGGER log, DATABASE* database, AUTH_DATA* auth_data){
	switch(auth_data->method){
		case AUTH_PLAIN:
			return auth_method_plain(log, database, auth_data);
		default:
			//TODO call plugins
			break;
	}

	return -1;
}
