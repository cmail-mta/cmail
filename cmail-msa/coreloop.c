int core_loop(LOGGER log, CONNPOOL listeners, sqlite3* master){
	fd_set readfds;
	int maxfd;
	int status;
	unsigned i;
	bool shutdown_signal=false;
	CONNPOOL clients={
		.count = 0,
		.conns = NULL
	};

	while(!shutdown_signal){
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
		//FIXME signalmask
		status=pselect(maxfd+1, &readfds, NULL, NULL, NULL, NULL);

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
				client_process(log, &(clients.conns[i]));
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

	//TODO free connpool aux_data structures
	connpool_free(&clients);

	return 0;
}
