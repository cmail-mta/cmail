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
	bool current_multiline;
	ssize_t bytes, left;
	fd_set readfds;
	struct timeval tv;
	time_t read_begin=time(NULL);
	int i, c;

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
			//read from peer
			left=sizeof(conn_data->recv_buffer)-conn_data->recv_offset;

			if(left<2){
				//unterminated line
				logprintf(log, LOG_ERROR, "Received response line with %d bytes, bailing out\n", conn_data->recv_offset);
				return -1;
			}
	
			bytes=network_read(log, conn, conn_data->recv_buffer+conn_data->recv_offset, left);

			//failed to read from socket
			if(bytes<0){
				#ifndef CMAIL_NO_TLS
				switch(conn->tls_mode){
					case TLS_NONE:
				#endif
				switch(errno){
					case EAGAIN:
						logprintf(log, LOG_WARNING, "Read signaled, but blocked\n");
						continue;
					default:
						logprintf(log, LOG_ERROR, "Failed to read from client: %s\n", strerror(errno));
						return -1;
				}
				#ifndef CMAIL_NO_TLS
					break;
					case TLS_NEGOTIATE:
						//errors during TLS negotiation
						if(bytes==-2){
							logprintf(log, LOG_ERROR, "TLS negotiation failed\n");
							return -1;
						}
						continue;
					case TLS_ONLY:
						switch(bytes){
							case GNUTLS_E_INTERRUPTED:
							case GNUTLS_E_AGAIN:
								logprintf(log, LOG_WARNING, "TLS read signaled, but blocked\n");
								continue;
							default:
								logprintf(log, LOG_ERROR, "GnuTLS reported an error while reading: %s\n", gnutls_strerror(bytes));
								return -1;
						}
				}
				#endif
			}
	
			//client disconnect / handshake success
			else if(bytes==0){
				#ifndef CMAIL_NO_TLS
				switch(conn->tls_mode){
					case TLS_NEGOTIATE:
						//tls handshake ok
						conn->tls_mode=TLS_ONLY;
						break;
					default:
				#endif
				logprintf(log, LOG_INFO, "Peer disconnected\n");
				return -1;
				#ifndef CMAIL_NO_TLS
				}
				#endif
				continue;
			}

			logprintf(log, LOG_DEBUG, "Received %d bytes of data, recv_offset is %d\n", bytes, conn_data->recv_offset);
			logprintf(log, LOG_ALL_IO, ">> %.*s", bytes, conn_data->recv_buffer+conn_data->recv_offset);

			//scan for terminator
			for(i=0;i<bytes-1;i++){
				//check for leading status code
				if(conn_data->recv_offset+i<3 
					&& !isdigit(conn_data->recv_buffer[conn_data->recv_offset+i])){
					logprintf(log, LOG_WARNING, "Response does not begin with status code, not a valid SMTP reply\n");
					return -1;
				}

				if(conn_data->recv_buffer[conn_data->recv_offset+i] == '\r'
					&& conn_data->recv_buffer[conn_data->recv_offset+i+1] == '\n'){
					
					conn_data->recv_buffer[conn_data->recv_offset+i]=0;
					logprintf(log, LOG_DEBUG, "Input buffer sentence %s\n", conn_data->recv_buffer);
					
					current_multiline=false;

					//crude length check
					if(conn_data->recv_offset+i<4){//3x digit, 1xdelim, text
						logprintf(log, LOG_ERROR, "SMTP reply too short\n");
						return -1;
					}

					//read response code
					conn_data->reply.code=strtoul(conn_data->recv_buffer, NULL, 10);
					//crude sanity check
					if(conn_data->reply.code>999){
						logprintf(log, LOG_ERROR, "Reply status code is out of bounds\n");
						return -1;
					}

					//check for multiline
					current_multiline=(conn_data->recv_buffer[3]=='-');
					conn_data->reply.multiline=(conn_data->reply.multiline|current_multiline);

					//copy message into reply structure
					//TODO

					//copyback
					i+=2;
					for(c=0;i+c<bytes-1;c++){
						conn_data->recv_buffer[c]=conn_data->recv_buffer[i+c];
					}

					bytes-=i;
					conn_data->recv_offset=0;
					i=-1;

					//continue if not at end
					if(!current_multiline){
						return 0;
					}

					logprintf(log, LOG_DEBUG, "Current sentence is multiline response, continuing read\n");
				}
			}
		}
	}
	while(!abort_signaled);

	return -1;
}


