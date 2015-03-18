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

int auth_authorize(LOGGER log, DATABASE* database, char* user, char* password){
	return -1;
}

int auth_validate_plain(LOGGER log, DATABASE* database, AUTH_DATA* auth_data){
	int length, i;
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
	for(i=0;i<length;i++){
		if(auth_data->parameter[i]==0){
			if(!user){
				user=auth_data->parameter+i+1;
			}
			else if(!pass){
				pass=auth_data->parameter+i+1;
			}
		}
	}

	logprintf(log, LOG_DEBUG, "Client credentials (%d bytes): %s:%s\n", length, user, pass);

	return auth_authorize(log, database, user, pass);
}

int auth_validate(LOGGER log, DATABASE* database, AUTH_DATA* auth_data){
	switch(auth_data->method){
		case AUTH_PLAIN:
			return auth_validate_plain(log, database, auth_data);
		default:
			//TODO call plugins
			break;
	}

	return -1;
}
