int protocol_reply_reset(LOGGER log, SMTPREPLY* reply){
	SMTPREPLY empty_reply = {
		.code = 0,
		.multiline = false,
		.response_text = reply->response_text,
		.buffer_length = reply->buffer_length
	};

	*reply=empty_reply;

	return 0;
}

int protocol_read(LOGGER log, CONNECTION* conn, int timeout){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;
	int status;
	fd_set readfds;
	struct timeval tv;
	time_t read_begin=time(NULL);

	protocol_reply_reset(log, &(conn_data->reply));
	
	do{
		tv.tv_sec=CMAIL_SELECT_INTERVAL;
		tv.tv_usec=0;

		FD_ZERO(&readfds);
		FD_SET(conn->fd, &readfds);

		status=select(conn->fd+1, &readfds, NULL, NULL, &tv);
		if(status<0){
			logprintf(log, LOG_ERROR, "select failed: %s", strerror(errno));
			return -1;
		}

		if(status==0){
			//check timeout
			if((time(NULL)-read_begin)>timeout){
				logprintf(log, LOG_WARNING, "Read timed out\n");
				return -1;
			}
		}
		
		#ifndef CMAIL_NO_TLS
		if(FD_ISSET(conn->fd, &readfds) || gnutls_record_check_pending(conn->tls_session)){
		#else
		if(FD_ISSET(conn->fd, &readfds)){
		#endif
			//TODO read from peer

		}

	}
	while(!abort_signaled);

	return -1;
}


