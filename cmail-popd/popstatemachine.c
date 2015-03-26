int state_authorization(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
	if(!strncasecmp(client_data->recv_buffer, "capa", 4)){
		return pop_capa(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		return pop_quit(log, client, database);
	}

	if(!strncasecmp(client_data->recv_buffer, "stls", 4)){
		//TODO
	}

	if(!strncasecmp(client_data->recv_buffer, "xyzzy", 5)){
		return pop_xyzzy(log, client, database);
	}

	//TODO disable this on tls-required auth
	if(!strncasecmp(client_data->recv_buffer, "user ", 5)){
		logprintf(log, LOG_INFO, "Client sends user %s\n", client_data->recv_buffer+5);
		client_data->auth=AUTH_USER;
		client_data->user=common_strdup(client_data->recv_buffer+5);
		if(!client_data->user){
			logprintf(log, LOG_WARNING, "Failed to allocate memory for user name\n");
			client_send(log, client, "-ERR Out of memory\r\n");
			return -1;
		}

		client_send(log, client, "-OK Go ahead\r\n");
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "pass ", 5)){
		//TODO
	}

	if(!strncasecmp(client_data->recv_buffer, "auth ", 5)){
		//TODO
	}

	//TODO parse response if in auth_sasl mode

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
		//TODO stat
	}

	if(!strncasecmp(client_data->recv_buffer, "list", 4)){
		//TODO list
	}
	
	if(!strncasecmp(client_data->recv_buffer, "retr", 4)){
		//TODO retr
	}

	if(!strncasecmp(client_data->recv_buffer, "dele", 4)){
		//TODO dele
	}

	if(!strncasecmp(client_data->recv_buffer, "rset", 4)){
		//TODO rset
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
