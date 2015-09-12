int control_accept(LOGGER log, int fd, CONNPOOL* clients){
	int clientfd = -1, client_slot = -1;
	long flags;

	if(connpool_active(*clients) >= CMAIL_MAX_CONCURRENT_CLIENTS){
		logprintf(log, LOG_INFO, "Not accepting new control client, limit reached\n");
		return -1;
	}

	clientfd = accept(fd, NULL, NULL);
	if(clientfd >= 0){
		client_slot = connpool_add(clients, clientfd);
	}

	if(client_slot < 0){
		logprintf(log, LOG_ERROR, "Failed to add control client to pool\n");
		return -1;
	}

	//set socket nonblocking
	flags = fcntl(clients->conns[client_slot].fd, F_GETFL, 0);
	if(flags < 0){
		flags = 0;
	}

	//FIXME check errno
	fcntl(clients->conns[client_slot].fd, F_SETFL, flags | O_NONBLOCK);
	return 0;
}

int control_close(LOGGER log, CONNECTION* control){
	close(control->fd);
	control->fd = -1;
	return 0;
}

int control_process(LOGGER log, CONNECTION* control){



	return 0;
}
