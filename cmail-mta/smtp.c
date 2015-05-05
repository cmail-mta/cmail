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
		return 1;
	}

	if(conn_data->reply.code==250){
		return 0;
	}

	logprintf(log, LOG_WARNING, "Recipient response code %d\n", conn_data->reply.code);

	if(conn_data->reply.code >= 400 && conn_data->reply.code <= 499){
		return 1;
	}

	return -1;
}

int smtp_data(LOGGER log, CONNECTION* conn, char* mail_data){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;
	char* mail_bytestuff;
	
	//Calling contract: retn 0 -> accepted, 1 -> fail temp, -1 -> fail perm
	client_send(log, conn, "DATA\r\n");

	if(protocol_expect(log, conn, SMTP_DATA_TIMEOUT, 354)){
		//FIXME might wanna test for other responses here
		logprintf(log, LOG_WARNING, "Data initiation failed, response was %d\n", conn_data->reply.code);
		return -1;
	}
	
	if(mail_data[0]=='.'){
		client_send(log, conn, ".");
	}
	do{
		mail_bytestuff=strstr(mail_data, "\r\n.");
		if(mail_bytestuff){
			client_send_raw(log, conn, mail_data, mail_bytestuff-mail_data);
			client_send(log, conn, "\r\n..");
			mail_data=mail_bytestuff+3;
		}
		else{
			//logprintf(log, LOG_DEBUG, "Sending %d bytes message data\n", strlen(mail_data));
			client_send_raw(log, conn, mail_data, strlen(mail_data));
		}
	}
	while(mail_bytestuff);
	
	client_send(log, conn, "\r\n.\r\n");
	
	if(protocol_read(log, conn, SMTP_DATA_TERMINATION_TIMEOUT)<0){
		logprintf(log, LOG_ERROR, "Failed to read data terminator response\n");
		return 1;
	}

	if(conn_data->reply.code==250){
		logprintf(log, LOG_INFO, "Mail accepted\n");
		return 0;
	}

	logprintf(log, LOG_WARNING, "Data terminator response code %d\n", conn_data->reply.code);

	if(conn_data->reply.code >= 400 && conn_data->reply.code <= 499){
		return 1;
	}

	return -1;
}

int smtp_rset(LOGGER log, CONNECTION* conn){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;
	client_send(log, conn, "RSET\r\n");

	if(protocol_expect(log, conn, SMTP_220_TIMEOUT, 250)){ //FIXME the rfc does not define a timeout for this
		logprintf(log, LOG_WARNING, "Could not reset connection, response was %d\n", conn_data->reply.code);
		return -1;
	}
	return 0;
}

int smtp_noop(LOGGER log, CONNECTION* conn){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;
	client_send(log, conn, "NOOP\r\n");

	if(protocol_expect(log, conn, 20, 250)){ //FIXME the rfc does not define a timeout for this
		logprintf(log, LOG_WARNING, "Could not noop, response was %d\n", conn_data->reply.code);
		return -1;
	}
	return 0;
}

int smtp_negotiate(LOGGER log, MTA_SETTINGS settings, char* remote, CONNECTION* conn, REMOTE_PORT port){
	unsigned i;
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

	//do tls padding
	if(settings.tls_padding && conn->tls_mode==TLS_ONLY){
		i=rand()%settings.tls_padding;
		for(;i>0;i--){
			if(rand()%2){
				smtp_noop(log, conn);
			}
			else{
				smtp_rset(log, conn);
			}
		}
	}

	return 0;
}

int smtp_deliver_loop(LOGGER log, DATABASE* database, sqlite3_stmt* tx_statement, CONNECTION* conn){
	int status;
	unsigned transactions=0, mails_delivered=0, i;
	MAIL current_mail = {
		.recipients = 0,
		.rcpt = NULL,
		.envelopefrom = NULL,
		.length = 0,
		.data = NULL
	};

	do{
		status=sqlite3_step(tx_statement);
		switch(status){
			case SQLITE_ROW:
				//handle outbound mail
				if(mail_dbread(log, &current_mail, tx_statement)<0){
					logprintf(log, LOG_ERROR, "Failed to read transaction %s, database status %s\n", (char*)sqlite3_column_text(tx_statement, 0), sqlite3_errmsg(database->conn));
				}
				else{
					if(mail_dispatch(log, database, &current_mail, conn)<0){
						logprintf(log, LOG_WARNING, "Failed to dispatch transaction %s\n", (char*)sqlite3_column_text(tx_statement, 0));
					}

					//handle failcount increase and bounces
					for(i=0;i<current_mail.recipients;i++){
						switch(current_mail.rcpt[i].status){
							case RCPT_OK:
								//delivery done, remove from database
								if(sqlite3_bind_int(database->delete_mail, 1, current_mail.rcpt[i].dbid)!=SQLITE_OK){
										logprintf(log, LOG_WARNING, "Failed to bind deletion parameter %d: %s\n", current_mail.rcpt[i].dbid, sqlite3_errmsg(database->conn));
								
								}
								else{
									if(sqlite3_step(database->delete_mail)!=SQLITE_DONE){
										logprintf(log, LOG_WARNING, "Failed to delete delivered mail id %d: %s\n", current_mail.rcpt[i].dbid, sqlite3_errmsg(database->conn));
									}
									sqlite3_reset(database->delete_mail);
									sqlite3_clear_bindings(database->delete_mail);
								}
								mails_delivered++;
								break;
							case RCPT_FAIL_TEMPORARY:
								//TODO increase failcount, bounce if necessary
								break;
							case RCPT_FAIL_PERMANENT:
								//TODO bounce immediately
								break;
							case RCPT_READY:
								logprintf(log, LOG_WARNING, "Recipient %d not touched by dispatch loop\n", i);
								//TODO this happens if the peer rejects the MAIL command
								//increase failcount, insert reason
								break;
						}
					}
				}
				transactions++;

				mail_free(&current_mail);
				break;
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "Iteration done, handled %d mails in %d transactions\n", mails_delivered, transactions);
				break;
		}
	}
	while(status==SQLITE_ROW);

	sqlite3_reset(tx_statement);
	sqlite3_clear_bindings(tx_statement);

	return transactions;
}
