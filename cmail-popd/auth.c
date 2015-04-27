int auth_reset(AUTH_DATA* auth_data){
	AUTH_DATA empty = {
		.method = AUTH_USER,
		.user = NULL,
		.authorized = false
	};

	if(auth_data->user){
		free(auth_data->user);
		auth_data->user=NULL;
	}

	*auth_data = empty;
	return 0;
}
