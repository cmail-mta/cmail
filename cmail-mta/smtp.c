int smtp_greet(LOGGER log, CONNECTION* conn){
	//protocol_ehlo
		//await 250
	//if fail, protocol_helo
	//if fail, disconnect
	return -1;
}

int smtp_starttls(LOGGER log, CONNECTION* conn){
	return -1;
}

int smtp_negotiate(LOGGER log, MTA_SETTINGS settings, char* remote, CONNECTION* conn, REMOTE_PORT port){
	int status;
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
	
	//await 220
	if(protocol_read(log, conn, SMTP_220_TIMEOUT)<0){
		logprintf(log, LOG_ERROR, "Initial SMTP response failed or not properly formatted\n");
		return -1;
	}
	
	if(conn_data->reply.code!=220){
		logprintf(log, LOG_WARNING, "Server replied with %d: %s\n", conn_data->reply.code, conn_data->reply.response_text);
		return -1;
	}
	
	//negotiate smtp
	if(smtp_greet(log, conn)<0){
		logprintf(log, LOG_WARNING, "Failed to negotiate SMTP/ESMTP\n");
		return -1;
	}

	if(port.tls_mode==TLS_NEGOTIATE){
		//if requested, proto_starttls
		if(smtp_starttls(log, conn)<0){
			logprintf(log, LOG_ERROR, "Failed to negotiate TLS layer\n");
			return -1;
		}
	}

	return 0;
}

int smtp_deliver_loop(LOGGER log, DATABASE* database, sqlite3_stmt* data_statement, CONNECTION* conn){
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
