int protocol_negotiate(LOGGER log, MTA_SETTINGS settings, char* remote, CONNECTION* conn, REMOTE_PORT port){
	fd_set readfds;
	struct timeval tv;
	int status;
	char recv_buffer[SMTP_MAX_LINE_LENGTH];
	unsigned recv_offset=0;
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;

	if(conn_data->state==STATE_NEW){
		//initialize first-time data
		#ifndef CMAIL_NO_TLS
		if(conn->tls_mode==TLS_NONE){
			tls_init_clientpeer(log, conn, remote);
	
			if(port.tls_mode==TLS_ONLY){
				//perform handshake immediately
				do{
					status=gnutls_handshake(conn->tls_session);
					if(status){
						if(gnutls_error_is_fatal(status)){
							logprintf(log, LOG_WARNING, "Handshake failed: %s\n", gnutls_strerror(status));
							return -1;
						}
						logprintf(log, LOG_WARNING, "Handshake nonfatal: %s\n", gnutls_strerror(status));
					}
				}
				while(status && !gnutls_error_is_fatal(status));
				conn->tls_mode=TLS_ONLY;
			}
		}
		#endif
	}
	
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
			//TODO check timeout
		}
		
		#ifndef CMAIL_NO_TLS
		if(FD_ISSET(conn->fd, &readfds) || gnutls_record_check_pending(conn->tls_session)){
		#else
		if(FD_ISSET(conn->fd, &readfds)){
		#endif
			//read from peer
			logprintf(log, LOG_DEBUG, "Data ready\n");

		}

		//TODO check if desired state has been reached
	}
	while(!abort_signaled);

	return -1;
}

int protocol_deliver_loop(LOGGER log, DATABASE* database, sqlite3_stmt* data_statement, CONNECTION* conn){
	int status;
	unsigned delivered_mails=0;
	MAIL current_mail = {
		.ids = NULL,
		.remote = NULL,
		.mailhost = NULL,
		.recipients = NULL,
		.length = 0,
		.data = NULL
	};
	
	do{
		status=sqlite3_step(data_statement);
		switch(status){
			case SQLITE_ROW:
				//handle outbound mail
				if(mail_dbread(log, &current_mail, data_statement)<0){
					logprintf(log, LOG_ERROR, "Failed to handle mail with IDlist %s, database status %s\n", (char*)sqlite3_column_text(data_statement, 0), sqlite3_errmsg(database->conn));
				}
				else{
					if(mail_dispatch(log, database, &current_mail)<0){
						logprintf(log, LOG_WARNING, "Failed to dispatch mail with IDlist %s\n", (char*)sqlite3_column_text(data_statement, 0));
					}
					delivered_mails++;
				}
				mail_free(&current_mail);
				break;
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "Iteration done, handled %d mails\n", delivered_mails);
				break;
		}
	}
	while(status==SQLITE_ROW);

	sqlite3_reset(data_statement);
	sqlite3_clear_bindings(data_statement);

	return delivered_mails;
}
