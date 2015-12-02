int core_loop(LOGGER log, CONNPOOL listeners, DATABASE* database){
	fd_set readfds;
	struct timeval select_timeout;
	int maxfd;
	int status;
	unsigned i;
	CONNPOOL clients = {
		.count = 0,
		.conns = NULL
	};

	while(!abort_signaled){
		//clear listen fds
		FD_ZERO(&readfds);
		maxfd = -1;

		//add listen fds
		for(i = 0; i < listeners.count; i++){
			if(listeners.conns[i].fd >= 0){
				FD_SET(listeners.conns[i].fd, &readfds);
				if(listeners.conns[i].fd > maxfd){
					maxfd = listeners.conns[i].fd;
				}
			}
		}

		//add client fds
		for(i = 0; i < clients.count; i++){
			if(clients.conns[i].fd >= 0){
				FD_SET(clients.conns[i].fd, &readfds);
				if(clients.conns[i].fd > maxfd){
					maxfd = clients.conns[i].fd;
				}
			}
		}

		//reset timeout
		select_timeout.tv_sec = CMAIL_SELECT_TIMEOUT;
		select_timeout.tv_usec = 0;

		//select over fds
		status = select(maxfd + 1, &readfds, NULL, NULL, &select_timeout);

		if(status < 0){
			logprintf(log, LOG_ERROR, "Core select: %s\n", strerror(errno));
			break;
		}
		else{
			logprintf(log, LOG_DEBUG, "Data on %d sockets\n", status);
		}

		//check client fds
		for(i = 0; i < clients.count; i++){
			if(clients.conns[i].fd >= 0){
				if(FD_ISSET(clients.conns[i].fd, &readfds)){
					//handle data
					//FIXME handle return value
					client_process(log, &(clients.conns[i]), database);
				}
				else if(client_timeout(log, &(clients.conns[i]))){
					logprintf(log, LOG_WARNING, "Client timed out, disconnecting\n");
					client_send(log, &(clients.conns[i]), "* BYE Client connection timed out due to inactivity\r\n");
					client_close(log, &(clients.conns[i]), database);
				}
			}
		}

		//check listen fds
		for(i = 0; i < listeners.count; i++){
			if(listeners.conns[i].fd >= 0 && FD_ISSET(listeners.conns[i].fd, &readfds)){
				//handle new client
				//FIXME handle return value
				client_accept(log, &(listeners.conns[i]), &clients);
			}
		}
	}

	//close connected clients
	for(i = 0; i < clients.count; i++){
		if(clients.conns[i].fd >= 0){
			client_close(log, &(clients.conns[i]), database);
		}
	}

	//TODO free connpool aux_data structures
	connpool_free(&clients);

	return 0;
}
