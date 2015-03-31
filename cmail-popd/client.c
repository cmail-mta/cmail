int client_line(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;

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
	int client_slot=-1, flags;
	CLIENT empty_data = {
		.listener = listener,
		.recv_offset = 0,
		.state = STATE_AUTH,
		.maildrop = {
			.count = 0,
			.mails = NULL,
			.list_user = NULL,
			.fetch_user = NULL,
			.delete_user = NULL
		},
		#ifndef CMAIL_NO_TLS
		.tls_mode=TLS_NONE,
		#endif
		.auth = {
			.method = AUTH_USER,
			.user = NULL,
			.authorized = false
		}
	};
	CLIENT* actual_data;
	LISTENER* listener_data=(LISTENER*)listener->aux_data;

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
		//TODO
	}

	//initialise / reset client data structure
	actual_data=(CLIENT*)clients->conns[client_slot].aux_data;
	*actual_data = empty_data;
	
	#ifndef CMAIL_NO_TLS
	//if on tlsonly port, immediately wait for negotiation
	if(listener_data->tls_mode==TLS_ONLY){
		logprintf(log, LOG_INFO, "Listen socket is TLSONLY, waiting for negotiation...\n");
		actual_data->tls_mode=TLS_NEGOTIATE;
		return tls_initclient(log, clients->conns[client_slot].fd, actual_data);
	}
	#endif
	
	client_send(log, &(clients->conns[client_slot]), "+OK %s POP3 ready\r\n", listener_data->announce_domain);
	return 0;
}

int client_close(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
	#ifndef CMAIL_NO_TLS
	//shut down the tls session
	if(client_data->tls_mode!=TLS_NONE){
		gnutls_bye(client_data->tls_session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(client_data->tls_session);
		client_data->tls_mode=TLS_NONE;
	}
	#endif

	//close the socket
	close(client->fd);

	//reset client data
	maildrop_release(log, database, &(client_data->maildrop), client_data->auth.user);
	auth_reset(&(client_data->auth));
	
	//return the conpool slot
	client->fd=-1;
	
	return 0;
}

int client_process(LOGGER log, CONNECTION* client, DATABASE* database){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;
	ssize_t left=sizeof(client_data->recv_buffer)-client_data->recv_offset;
	ssize_t bytes;
	unsigned c, status;
	int i;

	//TODO handle client timeout
	
	#ifndef CMAIL_NO_TLS
	do{
	left=sizeof(client_data->recv_buffer)-client_data->recv_offset;
	switch(client_data->tls_mode){
		case TLS_NONE:
			//non-tls client
			bytes=recv(client->fd, client_data->recv_buffer+client_data->recv_offset, left, 0);
			break;
		case TLS_NEGOTIATE:
			//tls handshake not completed
			status=gnutls_handshake(client_data->tls_session);
			if(status){
				if(gnutls_error_is_fatal(status)){
					logprintf(log, LOG_ERROR, "TLS Handshake reported fatal error: %s\n", gnutls_strerror(status));
					client_close(log, client, database);
					return -1;
				}
				logprintf(log, LOG_WARNING, "TLS Handshake reported nonfatal error: %s\n", gnutls_strerror(status));
				return 0;
			}
			client_data->tls_mode=TLS_ONLY;
			if(listener_data->tls_mode==TLS_ONLY){
				//send greeting if listener is tlsonly
				client_send(log, client, "+OK %s POP3 ready\r\n", listener_data->announce_domain);
			}
			logprintf(log, LOG_INFO, "TLS Handshake completed\n");
			return 0;
		case TLS_ONLY:
			//read with tls
			bytes=gnutls_record_recv(client_data->tls_session, client_data->recv_buffer+client_data->recv_offset, left);
			break;
	}
	#else
	bytes=recv(client->fd, client_data->recv_buffer+client_data->recv_offset, left, 0);
	#endif

	//failed to read from socket
	if(bytes<0){
		#ifndef CMAIL_NO_TLS
		switch(client_data->tls_mode){
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
				logprintf(log, LOG_WARNING, "This should not never have been reached\n");
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
	
	//client disconnect
	else if(bytes==0){
		logprintf(log, LOG_INFO, "Client has disconnected\n");
		client_close(log, client, database);
		return 0;
	}

	logprintf(log, LOG_DEBUG, "Received %d bytes of data, recv_offset is %d\n", bytes, client_data->recv_offset);

	//scan the newly received data for terminators
	for(i=0;i<bytes-1;i++){
		if(client_data->recv_offset+i>=POP_MAX_LINE_LENGTH-2){
			//TODO implement this properly
			//FIXME if in data mode, process the line anyway
			//else, skip until next newline (set flag to act accordingly in next read)
			logprintf(log, LOG_WARNING, "Line too long, handling current contents\n");
			client_send(log, client, "-ERR Line too long\r\n");
			client_data->recv_buffer[client_data->recv_offset+i]='\r';
			client_data->recv_buffer[client_data->recv_offset+i+1]='\n';
		}

		if(client_data->recv_buffer[client_data->recv_offset+i]=='\r' 
				&& client_data->recv_buffer[client_data->recv_offset+i+1]=='\n'){
			//terminate line
			client_data->recv_buffer[client_data->recv_offset+i]=0;
			//process by state machine (FIXME might use the return value for something)
			logprintf(log, LOG_DEBUG, "Extracted line spans %d bytes\n", client_data->recv_offset+i);
			client_line(log, client, database);
			//copyback
			i++;
			for(c=0;c+i<bytes-1;c++){
				//logprintf(log, LOG_DEBUG, "Moving character %02X from position %d to %d\n", client_data->recv_buffer[client_data->recv_offset+i+1+c], client_data->recv_offset+i+1+c, c);
				client_data->recv_buffer[c]=client_data->recv_buffer[client_data->recv_offset+i+1+c];
			}
			
			i=-1;
			bytes=c;

			//recv_offset points to the end of the last read, so 0 now
			client_data->recv_offset=0;
			
			logprintf(log, LOG_DEBUG, "recv_offset is now %d, first byte %02X\n", client_data->recv_offset, client_data->recv_buffer[0]);
		}
	}

	//update recv_offset with unprocessed bytes
	client_data->recv_offset+=bytes;
	#ifndef CMAIL_NO_TLS
	}
	while(client_data->tls_mode == TLS_ONLY && gnutls_record_check_pending(client_data->tls_session));
	#endif
	
	return 0;
}

