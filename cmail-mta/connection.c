int connection_reset(LOGGER log, CONNECTION* conn, bool data_valid){
	CONNDATA empty_data = {
		.extensions_supported = false,
		.last_action = time(NULL),
		.recv_buffer = "",
		.recv_offset = 0,
		.reply = {
			.code = 0,
			.multiline = false,
			.buffer_length = 0,
			.response_text = NULL
		}
	};

	CONNECTION empty_conn = {
		#ifndef CMAIL_NO_TLS
		.tls_mode = TLS_NONE,
		#endif
		.fd = -1,
		.aux_data = conn->aux_data
	};

	CONNDATA* conn_data;

	if(data_valid){
		logprintf(log, LOG_DEBUG, "Shutting down connection\n");

		#ifndef CMAIL_NO_TLS
		//shut down the tls session
		if(conn->tls_mode!=TLS_NONE){
			//this only shuts down the write direction, since gnutls_bye
			//blocks on a recv() with some peers not shutting down properly
			gnutls_bye(conn->tls_session, GNUTLS_SHUT_WR);
		}
		gnutls_deinit(conn->tls_session);
		#endif

		if(conn->fd>0){
			close(conn->fd);
		}
	}
	else{
		logprintf(log, LOG_DEBUG, "Initializing connection data\n");
	}

	*conn=empty_conn;

	if(conn->aux_data){
		conn_data=(CONNDATA*)conn->aux_data;

		if(data_valid){
			if(conn_data->reply.response_text){
				free(conn_data->reply.response_text);
			}
		}

		*conn_data=empty_data;
	}

	return 0;
}
