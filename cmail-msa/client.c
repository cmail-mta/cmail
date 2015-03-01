int client_line(LOGGER log, CONNECTION* client, sqlite3* master){
	logprintf(log, LOG_DEBUG, "Client processing of line started: %s\n", ((CLIENT*)client->aux_data)->recv_buffer);
	switch(((CLIENT*)client->aux_data)->state){
		case STATE_NEW:
			return smtpstate_new(log, client, master);
		case STATE_IDLE:
			return smtpstate_idle(log, client, master);
		case STATE_RECIPIENTS:
			return smtpstate_recipients(log, client, master);
		case STATE_DATA:
			return smtpstate_data(log, client, master);
		default:
			//TODO resolve to plugin handler
			break;
	}
	return 0;
}

int client_accept(LOGGER log, CONNECTION* listener, CONNPOOL* clients){
	int client_slot=-1;
	CLIENT client_data = {
		.listener=listener,
		.state=STATE_NEW,
		.recv_offset=0,
		.current_mail=NULL
	};

	if(connpool_active(*clients)>=MAX_SIMULTANEOUS_CLIENTS){
		logprintf(log, LOG_INFO, "Not accepting new client, limit reached\n");
		return 1;
	}
	
	client_slot=connpool_add(clients, accept(listener->fd, NULL, NULL));
	
	if(client_slot<0){
		logprintf(log, LOG_ERROR, "Failed to pool client socket\n");
		return -1;
	}

	if(!(clients->conns[client_slot].aux_data)){
		clients->conns[client_slot].aux_data=malloc(sizeof(CLIENT));
		if(!clients->conns[client_slot].aux_data){
			logprintf(log, LOG_ERROR, "Failed to allocate client data set\n");
			return -1;
		}
	}

	//initialise / reset client data structure
	(*((CLIENT*)clients->conns[client_slot].aux_data)) = client_data;

	//logprintf(log, LOG_DEBUG, "Initialized client data to offset %d\n", ((CLIENT*)clients->conns[client_slot].aux_data)->input_offset);

	send(clients->conns[client_slot].fd, "220 ", 4, 0);
	send(clients->conns[client_slot].fd, ((LISTENER*)listener->aux_data)->announce_domain, strlen(((LISTENER*)listener->aux_data)->announce_domain), 0);
	send(clients->conns[client_slot].fd, " service ready\r\n", 16, 0);
	return 0;
}

int client_close(CONNECTION* client){
	close(client->fd);
	client->fd=-1;
	//TODO return current mailbuffer
	return 0;
}

int client_process(LOGGER log, CONNECTION* client, sqlite3* master){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	size_t left=sizeof(client_data->recv_buffer)-client_data->recv_offset;
	ssize_t bytes;
	unsigned i, c;

	//TODO handle client timeout
	//TODO factor out writing operations

	bytes=recv(client->fd, client_data->recv_buffer+client_data->recv_offset, left, 0);

	//failed to read from socket
	if(bytes<0){
		logprintf(log, LOG_ERROR, "Failed to read from client: %s\n", strerror(errno));
		client_close(client);
		return -1;
	}
	//client disconnect
	else if(bytes==0){
		logprintf(log, LOG_INFO, "Client has disconnected\n");
		client_close(client);
		return 0;
	}

	logprintf(log, LOG_DEBUG, "Received %d bytes of data, recv_offset is %d\n", bytes, client_data->recv_offset);

	left-=bytes;
	if(left<2){
		logprintf(log, LOG_WARNING, "Connection read buffer exhausted, resetting\n");
		//TODO set errflag in order to ignore trailing characters in next read
		send(client->fd, "500 Line too long\r\n", 19, 0);
		client_data->recv_offset=0;
		return 0;
	}

	//scan the newly received data for terminators
	for(i=0;i<bytes-1;i++){
		if(client_data->recv_buffer[client_data->recv_offset+i]=='\r' 
				&& client_data->recv_buffer[client_data->recv_offset+i+1]=='\n'){
			//terminate line
			client_data->recv_buffer[client_data->recv_offset+i]=0;
			//process by state machine (FIXME might use the return value for something)
			logprintf(log, LOG_DEBUG, "Extracted line spans %d bytes\n", client_data->recv_offset+i);
			client_line(log, client, master);
			//copyback
			i++;
			for(c=0;c+i<bytes-1;c++){
				//logprintf(log, LOG_DEBUG, "Moving character %02X from position %d to %d\n", client_data->recv_buffer[client_data->recv_offset+i+1+c], client_data->recv_offset+i+1+c, c);
				client_data->recv_buffer[c]=client_data->recv_buffer[client_data->recv_offset+i+1+c];
			}
			
			i=-1;
			bytes=c;
			
			logprintf(log, LOG_DEBUG, "recv_offset is now %d, first byte %02X\n", client_data->recv_offset, client_data->recv_buffer[0]);
		}
	}

	//update recv_offset
	client_data->recv_offset+=bytes;
	
	return 0;
}

