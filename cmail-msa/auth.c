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

int auth_base64decode(char* in){
	return -1;	
}

int auth_validate_plain(LOGGER log, DATABASE* database, AUTH_DATA* auth_data){
	int length;
	
	if(!auth_data->parameter){
		return 1;
	}

	length=auth_base64decode(auth_data->parameter);

	if(length<0){
		logprintf(log, LOG_ERROR, "Failed to decode PLAIN authentication parameter\n");
		return -1;
	}

	logprintf(log, LOG_DEBUG, "Client credentials: %s\n", auth_data->parameter);

	return -1;
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
