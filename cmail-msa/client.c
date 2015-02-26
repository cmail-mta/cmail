int client_accept(LOGGER log, CONNECTION* listener, CONNPOOL* clients){
	int client_slot=-1;
	CLIENT client_data = {
		.listener=listener,
		.state=STATE_NEW,
		.recv_offset=0
	};
	
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

int client_process(LOGGER log, CONNECTION* client){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	size_t left=sizeof(client_data->recv_buffer)-client_data->recv_offset;
	ssize_t bytes;

	//TODO handle client timeout
	//TODO factor out writing operations

	bytes=recv(client->fd, client_data->recv_buffer+client_data->recv_offset, left, 0);

	//failed to read from socket
	if(bytes<0){
		logprintf(log, LOG_ERROR, "Failed to read from client: %s\n", strerror(errno));
		//close socket & release slot
		close(client->fd);
		client->fd=-1;
		return -1;
	}
	//client disconnect
	else if(bytes==0){
		logprintf(log, LOG_INFO, "Client has disconnected\n");
		//close socket & release slot
		close(client->fd);
		client->fd=-1;
		return 0;
	}

	logprintf(log, LOG_DEBUG, "Received %d bytes of data\n", bytes);

	left-=bytes;
	if(left<2){
		logprintf(log, LOG_WARNING, "Connection read buffer exhausted, resetting\n");
		//TODO scan until next newline
		send(client->fd, "500 Line too long\r\n", 19, 0);
		client_data->recv_offset=0;
		return 0;
	}


	//TODO scan recv_buffer for newlines, handle and shift, update recv_offset

	return 0;
}
