int core_loop(LOGGER log, CONNPOOL listeners, DATABASE* database){
	fd_set readfds;
	int maxfd;
	int status;
	unsigned i;
	CONNPOOL clients = {
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
		maxfd=-1;

		//add listen fds
		for(i=0;i<listeners.count;i++){
			if(listeners.conns[i].fd>0){
				FD_SET(listeners.conns[i].fd, &readfds);
				if(listeners.conns[i].fd>maxfd){
					maxfd=listeners.conns[i].fd;
				}
			}
		}

		//add client fds
		for(i=0;i<clients.count;i++){
			if(clients.conns[i].fd>0){
				FD_SET(clients.conns[i].fd, &readfds);
				if(clients.conns[i].fd>maxfd){
					maxfd=clients.conns[i].fd;
				}
			}
		}

		//select over fds
		status=select(maxfd+1, &readfds, NULL, NULL, NULL);

		if(status<=0){
			logprintf(log, LOG_ERROR, "Error in select: %s\n", strerror(errno));
			break;
		}
		else{
			logprintf(log, LOG_DEBUG, "Data on %d sockets\n", status);
		}

		//check client fds
		for(i=0;i<clients.count;i++){
			if(clients.conns[i].fd>0 && FD_ISSET(clients.conns[i].fd, &readfds)){
				//handle data
				//FIXME handle return value
				client_process(log, &(clients.conns[i]), database, &path_pool);
			}
		}

		//check listen fds
		for(i=0;i<listeners.count;i++){
			if(listeners.conns[i].fd>0 && FD_ISSET(listeners.conns[i].fd, &readfds)){
				//handle new client
				//FIXME handle return value
				client_accept(log, &(listeners.conns[i]), &clients);
			}
		}
	}

	//close connected clients
	for(i=0;i<clients.count;i++){
		if(clients.conns[i].fd>=0){
			client_close(&(clients.conns[i]));
		}
	}

	//TODO free connpool aux_data structures
	connpool_free(&clients);
	pathpool_free(&path_pool);

	return 0;
}
