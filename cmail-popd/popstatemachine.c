int state_authorization(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;
	
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
			return 0;
		}

		client_send(log, client, "+OK Start TLS negotiation\r\n");

		client->tls_mode=TLS_NEGOTIATE;
		return tls_init_serverpeer(log, client, listener_data->tls_priorities, listener_data->tls_cert);
	}
	#endif

	if(!strncasecmp(client_data->recv_buffer, "xyzzy", 5)){
		return pop_xyzzy(log, client, database);
	}

	#ifndef CMAIL_NO_TLS
	//disable login on tls-required auth
	if(client->tls_mode==TLS_ONLY || !listener_data->tls_require){
	#endif
		if(!strncasecmp(client_data->recv_buffer, "user ", 5)){
			if(client_data->auth.user){
				logprintf(log, LOG_INFO, "Client issued multiple USER commands\n");
				client_send(log, client, "-ERR User already set\r\n");
				return -1;
			}

			logprintf(log, LOG_INFO, "Client sends user %s\n", client_data->recv_buffer+5);
			client_data->auth.method=AUTH_USER;
			client_data->auth.user=common_strdup(client_data->recv_buffer+5);
			if(!client_data->auth.user){
				logprintf(log, LOG_WARNING, "Failed to allocate memory for user name\n");
				client_send(log, client, "-ERR Out of memory\r\n");
				return -1;
			}

			client_send(log, client, "+OK Go ahead\r\n");
			return 0;
		}

		if(!strncasecmp(client_data->recv_buffer, "pass ", 5)){
			if(!client_data->auth.user || client_data->auth.method!=AUTH_USER){
				logprintf(log, LOG_WARNING, "Client tried PASS without user or in another method\n");
				client_send(log, client, "-ERR Not possible now\r\n");
				return -1;
			}

			client_data->auth.authorized=(auth_validate(log, database, client_data->auth.user, client_data->recv_buffer+5)==0);
			if(!client_data->auth.authorized){
				//failed to authenticate
				auth_reset(&(client_data->auth));
				client_send(log, client, "-ERR Login failed\r\n");
				return 0;
			}
			else{
				//authentication ok, try to acquire the maildrop
				if(maildrop_acquire(log, database, &(client_data->maildrop), client_data->auth.user)<0){
					auth_reset(&(client_data->auth));
					client_send(log, client, "-ERR Failed to lock the maildrop\r\n");
					return 0;
				}
				client_data->state=STATE_TRANSACTION;
				client_send(log, client, "+OK Lock and load\r\n");
				return 0;
			}
		}

		if(!strncasecmp(client_data->recv_buffer, "auth ", 5)){
			//TODO
		}

		//TODO parse response if in auth_sasl mode
	#ifndef CMAIL_NO_TLS
	}
	#endif
	client_send(log, client, "-ERR Unkown command\r\n");
	return -1;
}

int state_transaction(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
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
		return pop_list(log, client, database, strtoul(client_data->recv_buffer+4, NULL, 10));
	}
	
	if(!strncasecmp(client_data->recv_buffer, "retr", 4)){
		return pop_retr(log, client, database, strtoul(client_data->recv_buffer+4, NULL, 10));
	}

	if(!strncasecmp(client_data->recv_buffer, "dele", 4)){
		return pop_dele(log, client, database, strtoul(client_data->recv_buffer+4, NULL, 10));
	}

	if(!strncasecmp(client_data->recv_buffer, "uidl", 4)){
		return pop_uidl(log, client, database, strtoul(client_data->recv_buffer+4, NULL, 10));
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
	CLIENT* client_data=(CLIENT*)client->aux_data;

	//this should probably never be reached
	logprintf(log, LOG_WARNING, "Commands received while in UPDATE state\n");

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		return pop_quit(log, client, database);
	}

	client_send(log, client, "-ERR Unkown command\r\n");
	return -1;
}
