int client_line(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	logprintf(log, LOG_ALL_IO, ">> %s\n", client_data->recv_buffer);
	switch(client_data->state){
		case STATE_NEW:
			return smtpstate_new(log, client, database, path_pool);
		case STATE_IDLE:
			return smtpstate_idle(log, client, database, path_pool);
		case STATE_AUTH:
			return smtpstate_auth(log, client, database, path_pool);
		case STATE_RECIPIENTS:
			return smtpstate_recipients(log, client, database, path_pool);
		case STATE_DATA:
			return smtpstate_data(log, client, database, path_pool);
		default:
			//TODO resolve to plugin handler
			break;
	}
	return 0;
}

int client_resolve(LOGGER log, CONNECTION* client){
	struct sockaddr_storage data;
	socklen_t len=sizeof(struct sockaddr_storage);
	CLIENT* client_data=(CLIENT*)client->aux_data;
	int status;

	if(getpeername(client->fd, (struct sockaddr*)&data, &len)<0){
		logprintf(log, LOG_ERROR, "Failed to get peer name: %s\n", strerror(errno));
		return -1;
	}

	status=getnameinfo((struct sockaddr*)&data, len, client_data->peer_name, MAX_FQDN_LENGTH-1, NULL, 0, 0);
	if(status){
		logprintf(log, LOG_WARNING, "Failed to resolve peer: %s\n", gai_strerror(status));
		return -1;
	}

	logprintf(log, LOG_INFO, "Connection from %s\n", client_data->peer_name);

	return 0;
}

int client_accept(LOGGER log, CONNECTION* listener, CONNPOOL* clients){
	int client_slot=-1, flags;
	LISTENER* listener_data=(LISTENER*)listener->aux_data;
	CLIENT empty_data = {
		.listener=listener,
		.state=STATE_NEW,
		.recv_offset=0,
		.peer_name="",
		.current_mail = {
			.submitter = NULL,
			.reverse_path = {
				.in_transaction = false,
				.path = "",
				.resolved_user = NULL
			},
			.forward_paths = {
				NULL
			},
			.message_id = "",
			.protocol = "unknown",
			//these need to persist between clients
			.data_offset = 0,
			.data_allocated = 0,
			.data_max = listener_data->max_size,
			.data = NULL
		},
		.auth = {
			.method = AUTH_PLAIN,
			.user = NULL,
			.parameter = NULL,
			.challenge = NULL,
			.response = NULL
		}
	};
	CLIENT* actual_data;

	if(connpool_active(*clients)>=CMAIL_MAX_CONCURRENT_CLIENTS){
		logprintf(log, LOG_INFO, "Not accepting new client, limit reached\n");
		return 1;
	}

	client_slot=connpool_add(clients, accept(listener->fd, NULL, NULL));

	if(client_slot<0){
		logprintf(log, LOG_ERROR, "Failed to pool client socket\n");
		return -1;
	}

	//set socket nonblocking
	flags=fcntl(clients->conns[client_slot].fd, F_GETFL, 0);
	if(flags<0){
		flags=0;
	}
	//FIXME check errno
  	fcntl(clients->conns[client_slot].fd, F_SETFL, flags|O_NONBLOCK);

	if(!(clients->conns[client_slot].aux_data)){
		clients->conns[client_slot].aux_data=malloc(sizeof(CLIENT));
		if(!clients->conns[client_slot].aux_data){
			logprintf(log, LOG_ERROR, "Failed to allocate client data set\n");
			return -1;
		}
	}
	else{
		//preserve old data
		//FIXME does this work?
		actual_data=(CLIENT*)clients->conns[client_slot].aux_data;
		empty_data.current_mail.data_allocated=actual_data->current_mail.data_allocated;
		empty_data.current_mail.data=actual_data->current_mail.data;
	}

	//initialise / reset client data structure
	actual_data=(CLIENT*)clients->conns[client_slot].aux_data;
	*actual_data = empty_data;

	if(client_resolve(log, &(clients->conns[client_slot]))<0){
		logprintf(log, LOG_WARNING, "Peer resolution failed\n");
		//FIXME this might be bigger than we think
	}

	if(listener_data->fixed_user){
		actual_data->auth.user=common_strdup(listener_data->fixed_user);
		if(!actual_data->auth.user){
			logprintf(log, LOG_ERROR, "Failed to allocate memory for fixed user authentication data\n");
			//dont fail here, its not critical
		}
	}

	actual_data->current_mail.submitter=actual_data->peer_name;
	logprintf(log, LOG_DEBUG, "Initialized client data to peername %s, submitter %s\n", actual_data->peer_name, actual_data->current_mail.submitter);

	#ifndef CMAIL_NO_TLS
	//if on tlsonly port, immediately wait for negotiation
	if(listener->tls_mode==TLS_ONLY){
		logprintf(log, LOG_INFO, "Listen socket is TLSONLY, waiting for negotiation...\n");
		clients->conns[client_slot].tls_mode=TLS_NEGOTIATE;
		return tls_init_serverpeer(log, &(clients->conns[client_slot]), listener_data->tls_priorities, listener_data->tls_cert);
	}
	#endif

	client_send(log, &(clients->conns[client_slot]), "220 %s ESMTP service ready\r\n", listener_data->announce_domain);
	return 0;
}

int client_close(CONNECTION* client){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	#ifndef CMAIL_NO_TLS
	//shut down the tls session
	if(client->tls_mode!=TLS_NONE){
		gnutls_bye(client->tls_session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(client->tls_session);
		client->tls_mode=TLS_NONE;
	}
	#endif

	//close the socket
	close(client->fd);

	//reset mail buffer contents
	mail_reset(&(client_data->current_mail));

	//reset authentication
	auth_reset(&(client_data->auth));

	//return the conpool slot
	client->fd=-1;

	return 0;
}

int client_process(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;
	ssize_t left, bytes, line_length;

	//TODO handle client timeout

	#ifndef CMAIL_NO_TLS
	do{
	#endif
	left=sizeof(client_data->recv_buffer)-client_data->recv_offset;

	if(left<2){
		//unterminated line
		//FIXME this might be kind of a harsh response
		logprintf(log, LOG_WARNING, "Line too long, closing client connection\n");
		client_send(log, client, "500 Line too long\r\n");
		client_close(client);
		return 0;
	}

	bytes=network_read(log, client, client_data->recv_buffer+client_data->recv_offset, left);

	//failed to read from socket
	if(bytes<0){
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
				client_close(client);
				return -1;
		}
		#ifndef CMAIL_NO_TLS
			break;
			case TLS_NEGOTIATE:
				//errors during TLS negotiation
				if(bytes==-2){
					client_close(client);
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
						client_close(client);
						return -1;
				}
		}
		#endif
	}

	//client disconnect / handshake success
	else if(bytes==0){
		#ifndef CMAIL_NO_TLS
		switch(client->tls_mode){
			case TLS_NEGOTIATE:
				//tls handshake ok
				client->tls_mode=TLS_ONLY;
				if(client_data->listener->tls_mode==TLS_ONLY){
					//send greeting if listener is tlsonly
					client_send(log, client, "220 %s ESMTPS service ready\r\n", listener_data->announce_domain);
				}
				break;
			default:
		#endif
		logprintf(log, LOG_INFO, "Client has disconnected\n");
		client_close(client);
		#ifndef CMAIL_NO_TLS
		}
		#endif
		return 0;
	}

	logprintf(log, LOG_DEBUG, "Received %d bytes of data, recv_offset is %d\n", bytes, client_data->recv_offset);

	do{
		line_length=common_next_line(log, client_data->recv_buffer, &(client_data->recv_offset), &bytes);
		if(line_length>=0){
			if(line_length>=SMTP_MAX_LINE_LENGTH-2){
				logprintf(log, LOG_WARNING, "Line too long, ignoring\n");
				client_send(log, client, "500 Line too long\r\n");
				//client_line(log, client, database, path_pool);
				//FIXME might handle this more sensibly
			}
			else{
				client_line(log, client, database, path_pool);
			}
		}
	}
	while(line_length>=0);

	#ifndef CMAIL_NO_TLS
	}
	while(client->tls_mode == TLS_ONLY && gnutls_record_check_pending(client->tls_session));
	#endif

	return 0;
}
