int smtp_greet(LOGGER log, CONNECTION* conn, MTA_SETTINGS settings){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;

	//try esmtp
	client_send(log, conn, "EHLO %s\r\n", settings.helo_announce);

	//await answer
	switch(protocol_expect(log, conn, SMTP_220_TIMEOUT, 250)){
		case 0:
			logprintf(log, LOG_DEBUG, "Negotiated ESMTP\n");
			conn_data->extensions_supported=true;
			return 0;

		case -1:
			logprintf(log, LOG_ERROR, "Failed to read EHLO response\n");
			return -1;
		default:
			break;
	}

	logprintf(log, LOG_INFO, "EHLO failed with %d, trying HELO\n", conn_data->reply.code);
	client_send(log, conn, "HELO %s\r\n", settings.helo_announce);

	if(protocol_expect(log, conn, SMTP_220_TIMEOUT, 250)){ //FIXME the rfc does not define a timeout for this
		logprintf(log, LOG_WARNING, "Could not negotiate any protocol, HELO response was %d\n", conn_data->reply.code);
		return -1;
	}

	logprintf(log, LOG_INFO, "Negotiated SMTP\n");
	return 0;
}

int smtp_starttls(LOGGER log, CONNECTION* conn){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;

	if(!conn_data->extensions_supported){
		logprintf(log, LOG_ERROR, "Extensions not supported, not negotiating TLS\n");
		return -1;
	}

	client_send(log, conn, "STARTTLS\r\n");

	if(protocol_expect(log, conn, SMTP_220_TIMEOUT, 220)){ //FIXME the rfc does not define a timeout for this
		logprintf(log, LOG_WARNING, "Could not start TLS negotiation, response was %d\n", conn_data->reply.code);
		return -1;
	}

	//perform handshake
	if(tls_handshake(log, conn)<0){
		logprintf(log, LOG_ERROR, "Failed to negotiate TLS\n");
		return -1;
	}

	return 0;
}

int smtp_initiate(LOGGER log, CONNECTION* conn, MAIL* mail){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;
	if(!mail->envelopefrom){
		logprintf(log, LOG_ERROR, "Mail did not have valid envelope sender\n");
		return -1;
	}

	client_send(log, conn, "MAIL FROM:<%s>\r\n", mail->envelopefrom);
	if(protocol_read(log, conn, SMTP_MAIL_TIMEOUT)<0){
		logprintf(log, LOG_ERROR, "Failed to read response to mail initiation\n");
		return -1;
	}

	if(conn_data->reply.code==250){
		return 0;
	}

	logprintf(log, LOG_WARNING, "Mail initiation response code %d\n", conn_data->reply.code);
	return -1;
}

int smtp_rcpt(LOGGER log, CONNECTION* conn, char* path){
	//Calling contract: retn 0 -> accepted, 1 -> fail temp, -1 -> fail perm
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;

	client_send(log, conn, "RCPT TO:<%s>\r\n", path);

	if(protocol_read(log, conn, SMTP_RCPT_TIMEOUT)<0){
		logprintf(log, LOG_ERROR, "Failed to read response to recipient\n");
		return -1;
	}

	if(conn_data->reply.code==250){
		return 0;
	}

	logprintf(log, LOG_WARNING, "Recipient response code %d\n", conn_data->reply.code);

	if(conn_data->reply.code >= 400 && conn_data->reply.code <= 500){
		return 1;
	}

	return -1;
}

int smtp_negotiate(LOGGER log, MTA_SETTINGS settings, char* remote, CONNECTION* conn, REMOTE_PORT port){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;

	//if(conn_data->state==STATE_NEW){
		//initialize first-time data
		#ifndef CMAIL_NO_TLS
		if(conn->tls_mode==TLS_NONE){
			tls_init_clientpeer(log, conn, remote, settings.tls_credentials);

			if(port.tls_mode==TLS_ONLY){
				//perform handshake immediately
				if(tls_handshake(log, conn)<0){
					logprintf(log, LOG_ERROR, "Failed to negotiate TLS with TLSONLY remote\n");
					return -1;
				}
			}
		}
		#endif
	//}

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
	if(smtp_greet(log, conn, settings)<0){
		logprintf(log, LOG_WARNING, "Failed to negotiate SMTP/ESMTP\n");
		return -1;
	}

	#ifndef CMAIL_NO_TLS
	if(port.tls_mode==TLS_NEGOTIATE && conn->tls_mode==TLS_NONE){
		//if requested, proto_starttls
		if(smtp_starttls(log, conn)<0){
			logprintf(log, LOG_ERROR, "Failed to negotiate TLS layer\n");
			return -1;
		}
		
		//perform greeting again
		if(smtp_greet(log, conn, settings)<0){
			logprintf(log, LOG_WARNING, "Failed to negotiate SMTPS/ESMTPS\n");
			return -1;
		}
	}
	#endif

	//TODO do tls padding

	return 0;
}

int smtp_deliver_loop(LOGGER log, DATABASE* database, sqlite3_stmt* data_statement, CONNECTION* conn){
	int status;
	unsigned delivered_mails=0;
	MAIL current_mail = {
		.recipients = 0,
		.rcpt = NULL,
		.envelopefrom = NULL,
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
					if(mail_dispatch(log, database, &current_mail, conn)<0){
						logprintf(log, LOG_WARNING, "Failed to dispatch mail with IDlist %s\n", (char*)sqlite3_column_text(data_statement, 0));
					}
					//TODO handle failcount increase and bounces
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
