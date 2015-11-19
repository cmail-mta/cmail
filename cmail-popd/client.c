int client_line(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;

	logprintf(log, LOG_ALL_IO, ">> %s\n", client_data->recv_buffer);

	switch(client_data->state){
		case STATE_AUTH:
			return state_authorization(log, client, database);
		case STATE_TRANSACTION:
			return state_transaction(log, client, database);
		case STATE_UPDATE:
			return state_update(log, client, database);
	}

	return 0;
}

int client_accept(LOGGER log, CONNECTION* listener, CONNPOOL* clients){
	int client_slot = -1, flags, status;
	CLIENT empty_data = {
		.listener = listener,
		.recv_offset = 0,
		.last_action = time(NULL),
		.connection_score = 0,
		.state = STATE_AUTH,
		.maildrop = {
			.count = 0,
			.mails = NULL,
			.conn = NULL,
			.list_user = NULL,
			.fetch_user = NULL,
			.mark_deletion = NULL,
			.unmark_deletions = NULL,
			.delete_user = NULL
		},
		.auth = {
			.method = AUTH_USER,
			.auth_ok = false,
			.ctx = {
				.method = SASL_INVALID
				//rest handled by sasl_begin
			},
			.user = {
				.authenticated  = NULL,
				.authorized = NULL
			}
		}
	};
	CLIENT* actual_data;
	LISTENER* listener_data = (LISTENER*)listener->aux_data;

	if(connpool_active(*clients) >= CMAIL_MAX_CONCURRENT_CLIENTS){
		logprintf(log, LOG_INFO, "Not accepting new client, limit reached\n");
		return 1;
	}

	client_slot = connpool_add(clients, accept(listener->fd, NULL, NULL));

	if(client_slot < 0){
		logprintf(log, LOG_ERROR, "Failed to pool client socket\n");
		return -1;
	}

	//set socket nonblocking
	flags = fcntl(clients->conns[client_slot].fd, F_GETFL, 0);
	if(flags < 0){
		flags=0;
	}
	status = fcntl(clients->conns[client_slot].fd, F_SETFL, flags | O_NONBLOCK);
	if(status < 0){
		logprintf(log, LOG_ERROR, "Failed to make client socket nonblocking: %s\n", strerror(errno));
	}

	if(!(clients->conns[client_slot].aux_data)){
		clients->conns[client_slot].aux_data = malloc(sizeof(CLIENT));
		if(!clients->conns[client_slot].aux_data){
			logprintf(log, LOG_ERROR, "Failed to allocate client data set\n");
			return -1;
		}
	}
	else{
		//preserve old data
		//none
	}

	//initialise / reset client data structure
	actual_data = (CLIENT*)clients->conns[client_slot].aux_data;
	*actual_data = empty_data;

	#ifndef CMAIL_NO_TLS
	//if on tlsonly port, immediately wait for negotiation
	if(listener->tls_mode == TLS_ONLY){
		logprintf(log, LOG_INFO, "Listen socket is TLSONLY, waiting for negotiation...\n");
		clients->conns[client_slot].tls_mode = TLS_NEGOTIATE;
		return tls_init_serverpeer(log, &(clients->conns[client_slot]), listener_data->tls_priorities, listener_data->tls_cert);
	}
	#endif

	client_send(log, &(clients->conns[client_slot]), "+OK %s POP3 ready\r\n", listener_data->announce_domain);
	return 0;
}

bool client_timeout(LOGGER log, CONNECTION* client){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	int delta = time(NULL) - client_data->last_action;

	if(delta < 0){
		logprintf(log, LOG_ERROR, "Time reported an error or skipped ahead: %s\n", strerror(errno));
		return false;
	}

	logprintf(log, LOG_DEBUG, "Client has activity delta %d seconds\n", delta);
	return delta > POP_SERVER_TIMEOUT;
}

int client_close(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;

	#ifndef CMAIL_NO_TLS
	//shut down the tls session
	if(client->tls_mode != TLS_NONE){
		gnutls_bye(client->tls_session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(client->tls_session);
		client->tls_mode = TLS_NONE;
	}
	#endif

	//close the socket
	close(client->fd);

	//reset client data
	if(client_data->auth.auth_ok && client_data->auth.user.authenticated){
		maildrop_release(log, database, &(client_data->maildrop), client_data->auth.user.authorized);
	}
	auth_reset(&(client_data->auth));

	//return the conpool slot
	client->fd = -1;

	return 0;
}

int client_process(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data = (CLIENT*)client->aux_data;
	LISTENER* listener_data = (LISTENER*)client_data->listener->aux_data;
	ssize_t left, bytes, line_length;

	#ifndef CMAIL_NO_TLS
	do{
	#endif
	left = sizeof(client_data->recv_buffer) - client_data->recv_offset;

	if(left < 2){
		//unterminated line
		//FIXME this might be kind of a harsh response
		logprintf(log, LOG_WARNING, "Line too long, closing client connection\n");
		client_send(log, client, "-ERR Line too long\r\n");
		client_close(log, client, database);
		return 0;
	}

	bytes = network_read(log, client, client_data->recv_buffer+client_data->recv_offset, left);

	//failed to read from socket
	if(bytes < 0){
		#ifndef CMAIL_NO_TLS
		switch(client->tls_mode){
			case TLS_NONE:
		#endif
		switch(errno){
			case EAGAIN:
				logprintf(log, LOG_WARNING, "Read signaled, but blocked\n");
				return 0;
			default:
				logprintf(log, LOG_ERROR, "Failed to read from client: %s\n", strerror(errno));
				client_close(log, client, database);
				return -1;
		}
		#ifndef CMAIL_NO_TLS
			break;
			case TLS_NEGOTIATE:
				//errors during TLS negotiation
				if(bytes == -2){
					client_close(log, client, database);
				}
				return 0;
			case TLS_ONLY:
				switch(bytes){
					case GNUTLS_E_INTERRUPTED:
					case GNUTLS_E_AGAIN:
						logprintf(log, LOG_WARNING, "TLS read signaled, but blocked\n");
						return 0;
					default:
						logprintf(log, LOG_ERROR, "GnuTLS reported an error while reading: %s\n", gnutls_strerror(bytes));
						client_close(log, client, database);
						return -1;
				}
		}
		#endif
	}

	//client disconnect / handshake success
	else if(bytes == 0){
		#ifndef CMAIL_NO_TLS
		switch(client->tls_mode){
			case TLS_NEGOTIATE:
				//tls handshake ok
				client->tls_mode = TLS_ONLY;
				if(client_data->listener->tls_mode == TLS_ONLY){
					//send greeting if listener is tlsonly
					client_send(log, client, "+OK %s POP3 ready\r\n", listener_data->announce_domain);
				}
				break;
			default:
		#endif
		logprintf(log, LOG_INFO, "Client has disconnected\n");
		client_close(log, client, database);
		#ifndef CMAIL_NO_TLS
		}
		#endif
		return 0;
	}

	logprintf(log, LOG_DEBUG, "Received %d bytes of data, recv_offset is %d\n", bytes, client_data->recv_offset);

	do{
		line_length = common_next_line(log, client_data->recv_buffer, &(client_data->recv_offset), &bytes);
		if(line_length >= 0){
			if(line_length >= POP_MAX_LINE_LENGTH-2){
				logprintf(log, LOG_WARNING, "Line too long, ignoring\n");
				client_send(log, client, "-ERR Line too long\r\n");
				//client_line(log, client, database);
				//FIXME might handle this more sensibly
			}
			else{
				//update last action timestamp
				client_data->last_action = time(NULL);

				client_data->connection_score += client_line(log, client, database);

				//kick the client after too many failed commands
				if(client_data->connection_score < CMAIL_FAILSCORE_LIMIT){
					logprintf(log, LOG_WARNING, "Disconnecting client because of bad connection score\n");
					client_send(log, client, "-ERR Too many failed commands\r\n");
					client_close(log, client, database);
					return 0;
				}
			}
		}
	}
	while(line_length >= 0);

	#ifndef CMAIL_NO_TLS
	}
	while(client->tls_mode == TLS_ONLY && gnutls_record_check_pending(client->tls_session));
	#endif

	return 0;
}
