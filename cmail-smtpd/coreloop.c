int core_loop(LOGGER log, CONNPOOL listeners, DATABASE* database, int* control_sockets){
	fd_set readfds;
	struct timeval select_timeout;
	int maxfd;
	int status;
	unsigned i;
	CONNPOOL clients = {
		.count = 0,
		.conns = NULL
	};

	CONNPOOL control_clients = {
		.count = 0,
		.conns = NULL
	};

	PATHPOOL path_pool = {
		.count = 0,
		.paths = NULL
	};

	while(!abort_signaled){
		//clear listen fds
		FD_ZERO(&readfds);
		maxfd = -1;

		//if enabled, add the control socketinput fds
		if(control_sockets){
			for(i = 0; control_sockets[i] >= 0; i++){
				FD_SET(control_sockets[i], &readfds);
				if(control_sockets[i] > maxfd){
					maxfd = control_sockets[i];
				}
			}

			for(i = 0; i < control_clients.count; i++){
				if(control_clients.conns[i].fd >= 0){
					FD_SET(control_clients.conns[i].fd, &readfds);
					if(control_clients.conns[i].fd > maxfd){
						maxfd = control_clients.conns[i].fd;
					}
				}
			}
		}

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
			logprintf(log, LOG_ERROR, "Error in select: %s\n", strerror(errno));
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
					client_process(log, &(clients.conns[i]), database, &path_pool);
				}
				else if(client_timeout(log, &(clients.conns[i]))){
					logprintf(log, LOG_WARNING, "Client timed out, disconnecting\n");
					client_send(log, &(clients.conns[i]), "500 Timed out\r\n");
					client_close(&(clients.conns[i]));
				}
			}
			else{
				//periodically release unused memory
				client_memtimeout(log, &(clients.conns[i]));
			}
		}

		//check listen fds
		for(i = 0; i < listeners.count; i++){
			if(listeners.conns[i].fd >= 0 && FD_ISSET(listeners.conns[i].fd, &readfds)){
				//handle new client
				//FIXME handle return value
				client_accept(log, database, &(listeners.conns[i]), &clients);
			}
		}

		//check control sockets
		if(control_sockets){
			//add control clients
			for(i = 0; control_sockets[i] >= 0; i++){
				if(FD_ISSET(control_sockets[i], &readfds)){
					//handle control client connect
					control_accept(log, control_sockets[i], &control_clients);
				}
			}

			//check control clients
			for(i = 0; i < control_clients.count; i++){
				if(control_clients.conns[i].fd >= 0 && FD_ISSET(control_clients.conns[i].fd, &readfds)){
					control_process(log, &(control_clients.conns[i]));
				}
			}
		}
	}

	//close connected clients
	for(i = 0; i < clients.count; i++){
		if(clients.conns[i].fd >= 0){
			client_close(&(clients.conns[i]));
		}

		//free allocated data
		client_free(log, &(clients.conns[i]));
	}

	connpool_free(&control_clients);
	connpool_free(&clients);
	pathpool_free(&path_pool);

	return 0;
}
