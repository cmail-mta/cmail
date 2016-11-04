/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

int auth_base64decode(LOGGER log, char* in){
	uint32_t decode_buffer;
	int group, len, i;
	char* idx;

	char* base64_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	len = strlen(in);

	if(len % 4){
		logprintf(log, LOG_WARNING, "Input has invalid length for base64\n");
		return -1;
	}

	//decode to code point indices
	for(i = 0; i < len; i++){
		if(in[i] == '='){
			//'=' is only allowed as trailing character, so fail if it is within valid base64
			//this is marked MUST by some rfcs (5034)
			for(; i < len; i++){
				if(in[i] != '='){
					logprintf(log, LOG_WARNING, "Input string contains = as non-trailing character\n");
					return -1;
				}

				//need to decode all groups
				in[i] = 0;
			}
			break;
		}

		idx = index(base64_alphabet, in[i]);
		if(!idx){
			logprintf(log, LOG_WARNING, "Input string contains invalid characters\n");
			return -1;
		}
		in[i] = idx - base64_alphabet;
	}

	for(group = 0; group < (len / 4); group++){
		//stuff the buffer
		decode_buffer = 0 | (in[group * 4] << 18);
		decode_buffer |= (in[(group * 4) + 1] << 12);
		decode_buffer |= (in[(group * 4) + 2] << 6);
		decode_buffer |= (in[(group * 4) + 3]);

		//read back decoded characters
		in[(group * 3)] = (decode_buffer & 0xFF0000) >> 16;
		in[(group * 3) + 1] = (decode_buffer & 0xFF00) >> 8;
		in[(group * 3) + 2] = (decode_buffer & 0xFF);
		in[(group * 3) + 3] = 0;
	}

	return (group * 3) + 3;
}

//CAVEAT: this reallocs the data buffer
int auth_base64encode(LOGGER log, uint8_t** input, size_t data_len){
	//doing this myself because i want it to be mostly in-place
	//libnettle's API does not specify whether overlapping input/output regions are okay, so i guess not
	uint32_t encode_buffer;
	char* base64_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t encode_triplets = (data_len / 3) + ((data_len % 3) ? 1:0);
	size_t current_triplet = 0;

	//reallocate buffer to encoded length
	*input = realloc(*input, (encode_triplets * 4 + 1) * sizeof(uint8_t));
	if(!(*input)){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for base64 encoding\n");
		return -1;
	}
	memset((*input) + data_len, 0, (encode_triplets * 4 + 1) - data_len);

	//iterate over triplets (from end)
	for(current_triplet = 0; current_triplet < encode_triplets; current_triplet++){
		//encode
		encode_buffer = *((*input) + (encode_triplets - current_triplet - 1) * 3) << 16;
		encode_buffer |= *((*input) + (encode_triplets - current_triplet - 1) * 3 + 1) << 8;
		encode_buffer |= *((*input) + (encode_triplets - current_triplet - 1) * 3 + 2);

		*((*input) + (encode_triplets - current_triplet - 1) * 4) = base64_alphabet[(encode_buffer & 0xFC0000) >> 18];
		*((*input) + (encode_triplets - current_triplet - 1) * 4 + 1) = base64_alphabet[(encode_buffer & 0x03F000) >> 12];
		*((*input) + (encode_triplets - current_triplet - 1) * 4 + 2) = base64_alphabet[(encode_buffer & 0x0FC0) >> 6];
		*((*input) + (encode_triplets - current_triplet - 1) * 4 + 3) = base64_alphabet[(encode_buffer & 0x3F)];
	}

	return 0;
}

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

int auth_validate(LOGGER log, sqlite3_stmt* auth_data, char* user, char* password, char** authorized_identity){
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

	if(sqlite3_bind_text(auth_data, 1, user, -1, SQLITE_STATIC) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind auth data query parameter\n");
		sqlite3_reset(auth_data);
		sqlite3_clear_bindings(auth_data);
		return -1;
	}

	status = sqlite3_step(auth_data);
	switch(status){
		case SQLITE_ROW:
			user_salt = (char*)sqlite3_column_text(auth_data, 0);
			if(user_salt){
				stored_hash = index(user_salt, ':');
				if(!stored_hash){
					logprintf(log, LOG_INFO, "Rejecting authentication for user %s, database entry invalid\n", user);
					break;
				}

				//calculate credentials hash
				auth_hash(digest_b16, sizeof(digest_b16), user_salt, stored_hash - user_salt, password, strlen(password));

				if(!strcmp(stored_hash + 1, digest_b16)){
					auth_id = (char*)sqlite3_column_text(auth_data, 1);
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
			logprintf(log, LOG_INFO, "Unhandled return value from auth data query: %d\n", status);
			break;
	}

	sqlite3_reset(auth_data);
	sqlite3_clear_bindings(auth_data);

	return rv;
}
