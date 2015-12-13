int state_authorization(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	LISTENER* listener_data = (LISTENER*)client_data->listener->aux_data;

	int status = SASL_OK;
	char* method = NULL;
	char* parameter = NULL;
	char* challenge = NULL;

	#ifndef CMAIL_NO_TLS
	//disable login on tls-required auth
	if(client->tls_mode == TLS_ONLY || !listener_data->tls_require){
	#endif

		//handle sasl continuation
		if(client_data->auth.method == AUTH_SASL && !client_data->auth.auth_ok){
			if(!strcmp(client_data->recv_buffer, "*")){
				//cancel authentication
				logprintf(log, LOG_INFO, "Client cancelled authentication\n");
				sasl_cancel(&(client_data->auth.ctx));
				auth_reset(&(client_data->auth));
				client_send(log, client, "-ERR Authentication canceled\r\n");
				return 0;
			}
			else if(client_data->auth.ctx.method != SASL_INVALID){
				status = sasl_continue(log, &(client_data->auth.ctx), client_data->recv_buffer, &challenge);
				//do not return immediately, response handled below
			}
			else{
				//This should never happen
				logprintf(log, LOG_INFO, "Client in invalid SASL state, resetting\n");
				sasl_cancel(&(client_data->auth.ctx));
				auth_reset(&(client_data->auth));
				client_send(log, client, "-ERR Invalid state\r\n");
				return -1;
			}
		}

		//handle sasl initiation
		if(status == SASL_OK && !strncasecmp(client_data->recv_buffer, "auth ", 5)){
			client_data->auth.method = AUTH_SASL;

			//tokenize along spaces
			method = strtok(client_data->recv_buffer + 5, " ");
			if(method){
				parameter = strtok(NULL, " ");
				logprintf(log, LOG_DEBUG, "Beginning SASL with method %s parameter %s\n", method, parameter?parameter:"null");
				status = sasl_begin(log, &(client_data->auth.ctx), &(client_data->auth.user), method, parameter, &challenge);
			}
			else{
				logprintf(log, LOG_WARNING, "Client tried auth without supplying method\n");
				status = SASL_UNKNOWN_METHOD;
			}
		}

		//handle SASL outcome
		if(status != SASL_OK){
			switch(status){
				case SASL_ERROR_PROCESSING:
					logprintf(log, LOG_ERROR, "SASL processing error\r\n");
					auth_reset(&(client_data->auth));
					client_send(log, client, "-ERR SASL Internal Error\r\n");
					return -1;
				case SASL_ERROR_DATA:
					logprintf(log, LOG_ERROR, "SASL failed to parse data\r\n");
					auth_reset(&(client_data->auth));
					client_send(log, client, "-ERR SASL Invalid data provided\r\n");
					return -1;
				case SASL_UNKNOWN_METHOD:
					logprintf(log, LOG_WARNING, "Client tried unsupported authentication method: %s\n", client_data->recv_buffer + 5);
					auth_reset(&(client_data->auth));
					client_send(log, client, "-ERR Unknown SASL mechanism\r\n");
					return -1;
				case SASL_CONTINUE:
					logprintf(log, LOG_INFO, "Asking for SASL continuation\r\n");
					client_send(log, client, "+ %s\r\n", challenge ? challenge:"");
					return 0;
				case SASL_DATA_OK:
					sasl_reset_ctx(&(client_data->auth.ctx), true);

					//check auth data
					if(!challenge || auth_validate(log, database, client_data->auth.user.authenticated, challenge, &(client_data->auth.user.authorized)) < 0){
						//login failed
						auth_reset(&(client_data->auth));
						logprintf(log, LOG_INFO, "Client failed to authenticate\n");
						client_send(log, client, "-ERR Authentication failed\r\n");
						return -1;
					}

					client_data->auth.auth_ok = true; //FIXME is this actually needed?

					if(!client_data->auth.user.authorized){
						auth_reset(&(client_data->auth));
						logprintf(log, LOG_ERROR, "Failed to allocate memory for authorized user\n");
						client_send(log, client, "-ERR Failed to allocate memory\r\n");
						return 0;
					}

					logprintf(log, LOG_INFO, "Client authenticated as user %s\n", client_data->auth.user.authenticated);

					//authentication ok, try to acquire the maildrop
					if(maildrop_acquire(log, database, &(client_data->maildrop), client_data->auth.user.authorized) < 0){
						auth_reset(&(client_data->auth));
						client_send(log, client, "-ERR Failed to lock the maildrop\r\n");
						return 0;
					}

					client_data->state = STATE_TRANSACTION;
					client_send(log, client, "+OK Lock and load\r\n");

					//decrease the failscore
					return 1;
			}

			logprintf(log, LOG_ERROR, "Invalid branch reached in AUTH\n");
			return -1;
		}

		if(!strncasecmp(client_data->recv_buffer, "user ", 5)){
			if(client_data->auth.user.authenticated){
				logprintf(log, LOG_INFO, "Client issued multiple USER commands\n");
				client_send(log, client, "-ERR User already set\r\n");
				return -1;
			}

			logprintf(log, LOG_INFO, "Client sends user %s\n", client_data->recv_buffer + 5);
			client_data->auth.method = AUTH_USER;
			client_data->auth.user.authenticated = common_strdup(client_data->recv_buffer + 5);
			if(!client_data->auth.user.authenticated){
				logprintf(log, LOG_WARNING, "Failed to allocate memory for user name\n");
				client_send(log, client, "-ERR Out of memory\r\n");
				return -1;
			}

			client_send(log, client, "+OK Go ahead\r\n");
			return 0;
		}

		if(!strncasecmp(client_data->recv_buffer, "pass ", 5)){
			if(!client_data->auth.user.authenticated || client_data->auth.method != AUTH_USER){
				logprintf(log, LOG_WARNING, "Client tried PASS without user or in another method\n");
				client_send(log, client, "-ERR Not possible now\r\n");
				return -1;
			}

			if(auth_validate(log, database, client_data->auth.user.authenticated, client_data->recv_buffer + 5, &(client_data->auth.user.authorized)) < 0){
				//failed to authenticate
				logprintf(log, LOG_INFO, "Failed to authenticate client\n");
				auth_reset(&(client_data->auth));
				client_send(log, client, "-ERR Login failed\r\n");

				//increase failscore
				return -1;
			}

			client_data->auth.auth_ok = true; //FIXME is this actually needed?

			if(!client_data->auth.user.authorized){
				logprintf(log, LOG_ERROR, "Failed to allocate memory for authorization user name\n");
				auth_reset(&(client_data->auth));
				client_send(log, client, "-ERR Internal error\r\n");
				return 0;
			}

			//authentication ok, try to acquire the maildrop
			if(maildrop_acquire(log, database, &(client_data->maildrop), client_data->auth.user.authorized) < 0){
				auth_reset(&(client_data->auth));
				client_send(log, client, "-ERR Failed to lock the maildrop\r\n");
				return 0;
			}

			logprintf(log, LOG_INFO, "Client authenticated as user %s\n", client_data->auth.user.authenticated);
			client_data->state = STATE_TRANSACTION;
			client_send(log, client, "+OK Lock and load\r\n");

			//decrease the failscore
			return 1;
		}
	#ifndef CMAIL_NO_TLS
	}
	#endif

	if(!strncasecmp(client_data->recv_buffer, "capa", 4)){
		return pop_capa(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		return pop_quit(log, client, database);
	}

	#ifndef CMAIL_NO_TLS
	if(!strncasecmp(client_data->recv_buffer, "stls", 4)){
		if(client->tls_mode != TLS_NONE || client_data->listener->tls_mode != TLS_NEGOTIATE){
			logprintf(log, LOG_WARNING, "Client tried STARTTLS at wrong time\n");
			client_send(log, client, "-ERR Not possible now\r\n");
			return -1;
		}

		client_send(log, client, "+OK Start TLS negotiation\r\n");

		client->tls_mode = TLS_NEGOTIATE;

		//FIXME this should be properly handled with a conditional
		return tls_init_serverpeer(log, client, listener_data->tls_priorities, listener_data->tls_cert);
	}
	#endif

	if(!strncasecmp(client_data->recv_buffer, "xyzzy", 5)){
		return pop_xyzzy(log, client, database);
	}

	client_send(log, client, "-ERR Unkown command\r\n");
	return -1;
}

int state_transaction(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;

	if(!strncasecmp(client_data->recv_buffer, "capa", 4)){
		return pop_capa(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		return pop_quit(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "xyzzy", 5)){
		return pop_xyzzy(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "stat", 4)){
		return pop_stat(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "list", 4)){
		return pop_list(log, client, database, strtoul(client_data->recv_buffer + 4, NULL, 10));
	}

	if(!strncasecmp(client_data->recv_buffer, "retr", 4)){
		return pop_retr(log, client, database, strtoul(client_data->recv_buffer + 4, NULL, 10));
	}

	if(!strncasecmp(client_data->recv_buffer, "dele", 4)){
		return pop_dele(log, client, database, strtoul(client_data->recv_buffer + 4, NULL, 10));
	}

	if(!strncasecmp(client_data->recv_buffer, "uidl", 4)){
		return pop_uidl(log, client, database, strtoul(client_data->recv_buffer + 4, NULL, 10));
	}

	if(!strncasecmp(client_data->recv_buffer, "rset", 4)){
		return pop_rset(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "noop", 4)){
		logprintf(log, LOG_DEBUG, "Client noop\n");
		client_send(log, client, "+OK Nothing happens\r\n");
		return 0;
	}

	client_send(log, client, "-ERR Unkown command\r\n");
	return -1;
}

int state_update(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;

	//this should probably never be reached
	logprintf(log, LOG_WARNING, "Commands received while in UPDATE state\n");

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		return pop_quit(log, client, database);
	}

	client_send(log, client, "-ERR Unkown command\r\n");
	return -1;
}
