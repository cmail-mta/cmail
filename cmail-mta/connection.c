int connection_reset(CONNECTION* conn, bool initialize){
	CONNDATA empty_data = {
		.state = STATE_NEW,
		.last_action = time(NULL),
		.recv_buffer = "",
		.recv_offset = 0
	};

	CONNECTION empty_conn = {
		#ifndef CMAIL_NO_TLS
		.tls_mode = TLS_NONE,
		#endif
		.fd = -1,
		.aux_data = conn->aux_data
	};

	CONNDATA* conn_data;

	if(!initialize){
		#ifndef CMAIL_NO_TLS
		//shut down the tls session
		if(conn->tls_mode!=TLS_NONE){
			gnutls_bye(conn->tls_session, GNUTLS_SHUT_RDWR);
		}
		gnutls_deinit(conn->tls_session);
		#endif

		if(conn->fd>0){
			close(conn->fd);
		}
	}

	*conn=empty_conn;

	if(conn->aux_data){
		conn_data=(CONNDATA*)conn->aux_data;
		*conn_data=empty_data;
	}

	return 0;
}
