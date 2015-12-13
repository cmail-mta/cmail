int auth_reset(AUTH_DATA* auth_data){
	AUTH_DATA empty = {
		.method = IMAP_LOGIN,
		.auth_tag = NULL,

		.ctx = {
			.method = SASL_INVALID
			//rest handled by sasl_begin
		},
		.user = {
			.authorized = NULL,
			.authenticated = NULL
		}
	};

	if(auth_data->auth_tag){
		free(auth_data->auth_tag);
	}

	sasl_reset_user(&(auth_data->user), true);
	sasl_reset_ctx(&(auth_data->ctx), true);

	*auth_data = empty;
	return 0;
}
