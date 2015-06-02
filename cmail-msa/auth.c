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

	//TODO handle aliasing at this point
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
