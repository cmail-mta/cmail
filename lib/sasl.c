/* This file is part of the cmail project (http://cmail.rocks/)
 * (c) 2015 Fabian "cbdev" Stumpf
 * License: Simplified BSD (2-Clause)
 * For further information, consult LICENSE.txt
 */

void sasl_reset_ctx(SASL_CONTEXT* ctx, bool data_valid){
	SASL_CONTEXT empty={
		.method = SASL_INVALID,
		.method_data = NULL,
		.user = NULL
	};

	if(data_valid){
		if(ctx->method_data){
			//FIXME arbitrate this by method
			free(ctx->method_data);
		}
	}

	*ctx = empty;
}

void sasl_reset_user(SASL_USER* user, bool data_valid){
	SASL_USER empty = {
		.authorized = NULL,
		.authenticated = NULL
	};

	if(data_valid){
		if(user->authorized){
			free(user->authorized);
		}

		if(user->authenticated){
			free(user->authenticated);
		}
	}

	*user = empty;
}

SASL_METHOD sasl_scan_method(char* method){
	if(!strncasecmp(method, "plain", 5)){
		return SASL_PLAIN;
	}

	return SASL_INVALID;
}

int sasl_challenge(LOGGER log, SASL_CONTEXT* ctx, char** response){
	switch(ctx->method){
		case SASL_PLAIN:
			*response = NULL;
			return SASL_CONTINUE;
		default:
			logprintf(log, LOG_ERROR, "No challenge handler defined for SASL method\n");
			return SASL_ERROR_PROCESSING;
	}

	return SASL_ERROR_PROCESSING;
}

int sasl_continue(LOGGER log, SASL_CONTEXT* ctx, char* data, char** response){
	int length, off_user = 0, off_pass = 0;

	if(!response){
		logprintf(log, LOG_ERROR, "NULL response pointer in sasl_continue\n");
		return SASL_ERROR_PROCESSING;
	}

	switch(ctx->method){
		case SASL_PLAIN:
			if(!data){
				return SASL_ERROR_DATA;
			}

			length=auth_base64decode(log, data);

			if(length<4){ //2x NUL, 2x min 1 byte
				logprintf(log, LOG_ERROR, "Failed to decode PLAIN authentication parameter\n");
				return SASL_ERROR_DATA;
			}

			//RFC 4616 message = [authzid] UTF8NUL authcid UTF8NUL passwd
			//TODO test this thoroughly
			off_user = strlen(data) + 1;
			if(off_user >= length || strlen(data + off_user) < 1 || off_user + strlen(data + off_user) >= length){
				logprintf(log, LOG_ERROR, "SASL PLAIN user read out of bounds\n");
				return SASL_ERROR_DATA;
			}

			off_pass = off_user + strlen(data + off_user) + 1;
			if(off_pass >= length || off_pass + strlen(data + off_pass) >= length){
				logprintf(log, LOG_ERROR, "SASL PLAIN pass read out of bounds\n");
				return SASL_ERROR_DATA;
			}

			//FIXME do not print passwords
			logprintf(log, LOG_DEBUG, "SASL PLAIN user %s pass %s\n", data + off_user, data + off_pass);
			ctx->user->authenticated = common_strdup(data + off_user);
			if(!(ctx->user->authenticated)){
				logprintf(log, LOG_ERROR, "Failed to allocate memory for authenticated user\n");
				return SASL_ERROR_PROCESSING;
			}

			*response = data + off_pass;
			return SASL_DATA_OK;
		default:
			logprintf(log, LOG_ERROR, "No continuation handler defined for SASL method\n");
			return SASL_ERROR_PROCESSING;
	}

	return SASL_ERROR_PROCESSING;
}

int sasl_begin(LOGGER log, SASL_CONTEXT* ctx, SASL_USER* user, char* method, char* data, char** response){
	//sanity check
	if(!ctx || !user || !method || !response){
		return SASL_ERROR_PROCESSING;
	}

	sasl_reset_ctx(ctx, false);
	sasl_reset_user(user, false);
	ctx->user = user;

	//scan method
	ctx->method = sasl_scan_method(method);

	if(ctx->method == SASL_INVALID){
		return SASL_UNKNOWN_METHOD;
	}

	//if initial supplied, handle
	if(data){
		return sasl_continue(log, ctx, data, response);
	}

	return sasl_challenge(log, ctx, response);
}

int sasl_cancel(SASL_CONTEXT* ctx){
	sasl_reset_ctx(ctx, true);
	return 0;
}
