int core_loop(LOGGER log, CONNPOOL listeners, DATABASE* database){
	fd_set readfds;
	struct timeval select_timeout;
	int maxfd;
	int status;
	unsigned i;
	pthread_t queue_worker;

	THREAD_CONFIG thread_config = {
		.log = log,
		.feedback_pipe = {-1, -1}
	};

	CONNPOOL clients = {
		.count = 0,
		.conns = NULL
	};

	COMMAND_QUEUE command_queue = {
		.entries = NULL,
		.entries_length = 0,
		.queue_access = PTHREAD_MUTEX_INITIALIZER,
		.queue_dirty = PTHREAD_COND_INITIALIZER,
		.tail = NULL,
		.head = NULL
	};

	//initialize the command queue data structures
	if(commandqueue_initialize(log, &command_queue) < 0){
		logprintf(log, LOG_ERROR, "Failed to initialize command_queue\n");
		return -1;
	}

	thread_config.queue = &command_queue;

	//create feedback pipe to trigger command queue processing
	if(pipe(thread_config.feedback_pipe) < 0){
		logprintf(log, LOG_ERROR, "Failed to create queue worker feedback pipe: %s\n", strerror(errno));
		commandqueue_free(&command_queue);
		return -1;
	}

	//TODO set the pipefd nonblocking

	//run the queue worker
	if(pthread_create(&queue_worker, NULL, queueworker_coreloop, &thread_config) != 0){
		logprintf(log, LOG_ERROR, "Failed to start queue worker thread\n");
		commandqueue_free(&command_queue);
		close(thread_config.feedback_pipe[0]);
		close(thread_config.feedback_pipe[1]);
		return -1;
	}

	while(!abort_signaled){
		//clear listen fds
		FD_ZERO(&readfds);
		maxfd = -1;

		//add feedback pipe to the read set
		FD_SET(thread_config.feedback_pipe[0], &readfds);
		if(thread_config.feedback_pipe[0] > maxfd){
			maxfd = thread_config.feedback_pipe[0];
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
		//FIXME this needs to be low in order to have the queue checked regularly
		//alternatively, use a pipe-to-self construct
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
					client_process(log, &(clients.conns[i]), database, &command_queue);
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

		if(FD_ISSET(thread_config.feedback_pipe[0], &readfds)){
			//TODO read from pipefd
			//TODO run queue completion check here, including preemptive cross-connection notifications
		}
	}

	//close connected clients
	for(i = 0; i < clients.count; i++){
		if(clients.conns[i].fd >= 0){
			client_close(log, &(clients.conns[i]), database);
		}
	}

	//join the worker thread
	pthread_cond_signal(&(command_queue.queue_dirty));
	logprintf(log, LOG_DEBUG, "Waiting for worker thread to terminate\n");
	pthread_join(queue_worker, NULL);
	logprintf(log, LOG_DEBUG, "Worker thread terminated successfully\n");

	//TODO free connpool aux_data structures
	connpool_free(&clients);
	commandqueue_free(&command_queue);
	close(thread_config.feedback_pipe[0]);
	close(thread_config.feedback_pipe[1]);

	return 0;
}
