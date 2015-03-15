int client_line(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	logprintf(log, LOG_ALL_IO, ">> %s\n", ((CLIENT*)client->aux_data)->recv_buffer);
	switch(((CLIENT*)client->aux_data)->state){
		case STATE_NEW:
			return smtpstate_new(log, client, database, path_pool);
		case STATE_IDLE:
			return smtpstate_idle(log, client, database, path_pool);
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

int client_send(LOGGER log, CONNECTION* client, char* fmt, ...){
	va_list args;
	ssize_t bytes;
	char send_buffer[STATIC_SEND_BUFFER_LENGTH+1];
	CLIENT* client_data=(CLIENT*)client->aux_data;

	va_start(args, fmt);
	//FIXME check if the buffer was long enough, if not, allocate a new one
	vsnprintf(send_buffer, STATIC_SEND_BUFFER_LENGTH, fmt, args);
	
	#ifndef CMAIL_NO_TLS
	switch(client_data->tls_mode){
		case TLS_NONE:
			bytes=send(client->fd, send_buffer, strlen(send_buffer), 0);
			break;
		case TLS_NEGOTIATE:
			logprintf(log, LOG_WARNING, "Not sending data while negotiation is in progess\n");
			break;
		case TLS_ONLY:
			bytes=gnutls_record_send(client_data->tls_session, send_buffer, strlen(send_buffer));
			break;
	}
	#else
	bytes=send(client->fd, send_buffer, strlen(send_buffer), 0);
	#endif

	logprintf(log, LOG_ALL_IO, "<< %s", send_buffer);

	va_end(args);
	return bytes;
}

#ifndef CMAIL_NO_TLS
int client_starttls(LOGGER log, CONNECTION* client){
	int status;
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;

	client_data->tls_mode=TLS_NEGOTIATE;

	status=gnutls_init(&(client_data->tls_session), GNUTLS_SERVER);
	if(status){
		logprintf(log, LOG_WARNING, "Failed to initialize TLS session for client: %s\n", gnutls_strerror(status));
		return -1;
	}

	status=gnutls_priority_set(client_data->tls_session, listener_data->tls_priorities);
	if(status){
		logprintf(log, LOG_WARNING, "Failed to update priority set for client: %s\n", gnutls_strerror(status));
	}

	status=gnutls_credentials_set(client_data->tls_session, GNUTLS_CRD_CERTIFICATE, listener_data->tls_cert);
	if(status){
		logprintf(log, LOG_WARNING, "Failed to set credentials for client: %s\n", gnutls_strerror(status));
		return -1;
	}

	gnutls_certificate_server_set_request(client_data->tls_session, GNUTLS_CERT_IGNORE);
	
	gnutls_transport_set_int(client_data->tls_session, client->fd);

	return 0;
}
#endif

int client_accept(LOGGER log, CONNECTION* listener, CONNPOOL* clients){
	int client_slot=-1, flags;
	CLIENT empty_data = {
		.listener=listener,
		.state=STATE_NEW,
		.recv_offset=0,
		.peer_name="",
		#ifndef CMAIL_NO_TLS
		.tls_mode=TLS_NONE,
		#endif
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
			//these need to persist between clients
			.data_offset = 0,
			.data_allocated = 0,
			.data = NULL
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
	
	actual_data->current_mail.submitter=actual_data->peer_name;
	logprintf(log, LOG_DEBUG, "Initialized client data to peername %s, submitter %s\n", actual_data->peer_name, actual_data->current_mail.submitter);

	#ifndef CMAIL_NO_TLS
	//if on tlsonly port, immediately wait for negotiation
	if(listener_data->tls_mode==TLS_ONLY){
		logprintf(log, LOG_INFO, "Listen socket is TLSONLY, waiting for negotiation...\n");
		return client_starttls(log, &(clients->conns[client_slot])); 
	}
	#endif

	client_send(log, &(clients->conns[client_slot]), "220 %s ESMTP service ready\r\n", listener_data->announce_domain);
	return 0;
}

int client_close(CONNECTION* client){
	CLIENT* client_data=(CLIENT*)client->aux_data;

	#ifndef CMAIL_NO_TLS
	//shut down the tls session
	if(client_data->tls_mode!=TLS_NONE){
		gnutls_bye(client_data->tls_session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(client_data->tls_session);
	}
	#endif

	//close the socket
	close(client->fd);

	//reset mail buffer contents
	mail_reset(&(client_data)->current_mail);
	
	//return the conpool slot
	client->fd=-1;
	
	return 0;
}

int client_process(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	size_t left=sizeof(client_data->recv_buffer)-client_data->recv_offset;
	ssize_t bytes;
	unsigned i, c, status;

	//TODO handle client timeout
	
	#ifndef CMAIL_NO_TLS
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
					client_close(client);
					return -1;
				}
				logprintf(log, LOG_WARNING, "TLS Handshake reported nonfatal error: %s\n", gnutls_strerror(status));
				return 0;
			}
			client_data->tls_mode=TLS_ONLY;
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
		#else
		switch(errno){
			case EAGAIN:
				logprintf(log, LOG_WARNING, "Read signaled, but blocked\n");
				return 0;
			default:
				logprintf(log, LOG_ERROR, "Failed to read from client: %s\n", strerror(errno));
				client_close(client);
				return -1;
		}
		#endif
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
						client_close(client);
						return -1;
				}
		}
		#endif
	}
	//client disconnect
	else if(bytes==0){
		logprintf(log, LOG_INFO, "Client has disconnected\n");
		client_close(client);
		return 0;
	}

	logprintf(log, LOG_DEBUG, "Received %d bytes of data, recv_offset is %d\n", bytes, client_data->recv_offset);

	//scan the newly received data for terminators
	for(i=0;i<bytes-1;i++){
		if(client_data->recv_offset+i>=SMTP_MAX_LINE_LENGTH-2){
			//TODO implement this properly
			//FIXME if in data mode, process the line anyway
			//else, skip until next newline (set flag to act accordingly in next read)
			logprintf(log, LOG_WARNING, "Line too long, handling current contents\n");
			client_send(log, client, "500 Line too long\r\n");
			client_data->recv_buffer[client_data->recv_offset+i]='\r';
			client_data->recv_buffer[client_data->recv_offset+i+1]='\n';
		}

		if(client_data->recv_buffer[client_data->recv_offset+i]=='\r' 
				&& client_data->recv_buffer[client_data->recv_offset+i+1]=='\n'){
			//terminate line
			client_data->recv_buffer[client_data->recv_offset+i]=0;
			//process by state machine (FIXME might use the return value for something)
			logprintf(log, LOG_DEBUG, "Extracted line spans %d bytes\n", client_data->recv_offset+i);
			client_line(log, client, database, path_pool);
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
	
	return 0;
}

