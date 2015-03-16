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

int auth_validate(LOGGER log, DATABASE* database, AUTH_DATA* auth_data){
	return -1;
}
