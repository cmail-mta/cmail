int mail_dbread(LOGGER log, MAIL* mail, sqlite3_stmt* stmt){
	int entries=1;
	unsigned i;
	char* id_list=(char*)sqlite3_column_text(stmt, 0);
	
	for(i=0;i<strlen(id_list);i++){
		if(id_list[i]==','){
			entries++;
		}
	}

	mail->envelopefrom=common_strdup((char*)sqlite3_column_text(stmt, 1));
	if(!mail->envelopefrom){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for envelope sender\n");
		return -1;
	}

	logprintf(log, LOG_DEBUG, "%d id list entries in %s\n", entries, id_list);
	mail->recipients=entries;
	mail->rcpt=calloc(entries, sizeof(MAIL_RCPT));
	if(!mail->rcpt){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for ID list\n");
		return -1;
	}

	i=0;
	do{
		mail->rcpt[i].dbid=strtoul(id_list, &id_list, 10);
		mail->rcpt[i].status=RCPT_READY;
		logprintf(log, LOG_DEBUG, "Updated entry %d to id %d\n", i, mail->rcpt[i].dbid);
		i++;
		id_list++; //skip separating comma
	}
	while(i<entries);

	mail->length=sqlite3_column_int(stmt, 2);
	logprintf(log, LOG_DEBUG, "%d bytes of mail data\n", mail->length);

	//read maildata
	mail->data=common_strdup((char*)sqlite3_column_text(stmt, 3));
	if(!mail->data){
		logprintf(log, LOG_ERROR, "Failed to allocate memory for mail data\n");
		return -1;
	}

	return 0;
}

int mail_failure(LOGGER log, DATABASE* database, int dbid, char* message, bool fatal){
	int rv=0;

	if(!message){
		message="No server response";
	}

	if(sqlite3_bind_int(database->insert_bounce_reason, 1, dbid) != SQLITE_OK 
		|| sqlite3_bind_text(database->insert_bounce_reason, 2, message, -1, SQLITE_STATIC) != SQLITE_OK
		|| sqlite3_bind_int(database->insert_bounce_reason, 3, fatal?1:0)){
		logprintf(log, LOG_ERROR, "Failed to bind bounce reason parameter: %s\n", sqlite3_errmsg(database->conn));
		rv=-1;
	}
	else{
		switch(sqlite3_step(database->insert_bounce_reason)){
			case SQLITE_DONE:
				break;
			default:
				logprintf(log, LOG_ERROR, "Failed to insert bounce reason for mail %d: %s\n", dbid, sqlite3_errmsg(database->conn));
				rv=-1;
		}
	}

	sqlite3_reset(database->insert_bounce_reason);
	sqlite3_clear_bindings(database->insert_bounce_reason);
	return rv;
}

int mail_dispatch(LOGGER log, DATABASE* database, MAIL* mail, CONNECTION* conn){
	CONNDATA* conn_data=(CONNDATA*)conn->aux_data;
	unsigned i;
	bool continue_data=false;
	
	if(smtp_initiate(log, conn, mail)){
		logprintf(log, LOG_WARNING, "Failed to initiate mail transaction\n");
		return -1;
	}

	for(i=0;i<mail->recipients;i++){
		sqlite3_bind_int(database->query_rcpt, 1, mail->rcpt[i].dbid); //FIXME error check this
		switch(sqlite3_step(database->query_rcpt)){
			case SQLITE_DONE:
				logprintf(log, LOG_WARNING, "dbid %d does not exist anymore\n", mail->rcpt[i].dbid);
				mail->rcpt[i].status=RCPT_FAIL_PERMANENT;
				break;
			case SQLITE_ROW:
				switch(smtp_rcpt(log, conn, (char*)sqlite3_column_text(database->query_rcpt, 0))){
					case -1:
						logprintf(log, LOG_INFO, "Recipient %d: %s failed permanently: %s\n", i, (char*)sqlite3_column_text(database->query_rcpt, 0), conn_data->reply.response_text);
						mail_failure(log, database, mail->rcpt[i].dbid, conn_data->reply.response_text, true);
						mail->rcpt[i].status=RCPT_FAIL_PERMANENT;
						break;
					case 0:
						logprintf(log, LOG_INFO, "Recipient %d: %s accepted\n", i, (char*)sqlite3_column_text(database->query_rcpt, 0));
						mail->rcpt[i].status=RCPT_OK;
						continue_data=true;
						break;
					case 1:
						logprintf(log, LOG_INFO, "Recipient %d: %s failed temporarily\n", i, (char*)sqlite3_column_text(database->query_rcpt, 0));
						mail_failure(log, database, mail->rcpt[i].dbid, conn_data->reply.response_text, false);
						mail->rcpt[i].status=RCPT_FAIL_TEMPORARY;
						break;
				}
				break;
			default:
				logprintf(log, LOG_WARNING, "Mail recipient query failed, database status: %s\n", sqlite3_errmsg(database->conn));
				//FIXME return -1;
				break;
		}

		sqlite3_reset(database->query_rcpt);
		sqlite3_clear_bindings(database->query_rcpt);
	}

	//bail out in order not to get a 5XX for greylisted recipients
	if(!continue_data){
		return -1;
	}

	switch(smtp_data(log, conn, mail->data)){
		case -1:
			//set all recipients to permanent fail
			logprintf(log, LOG_WARNING, "Mail rejected\n");
			for(i=0;i<mail->recipients;i++){
				mail_failure(log, database, mail->rcpt[i].dbid, conn_data->reply.response_text, true);
				mail->rcpt[i].status=RCPT_FAIL_PERMANENT;
			}
			return -1;
		case 1:
			//set all recipients to temp fail
			for(i=0;i<mail->recipients;i++){
				mail_failure(log, database, mail->rcpt[i].dbid, conn_data->reply.response_text, false);
				mail->rcpt[i].status=RCPT_FAIL_TEMPORARY;
			}
			logprintf(log, LOG_WARNING, "Mail not accepted, deferring\n");
			return -1;
		case 0:
			logprintf(log, LOG_INFO, "Mail accepted\n");
			return 0;
	}

	return -1;
}

int mail_reset(MAIL* mail, bool data_valid){
	MAIL empty_mail = {
		.recipients = 0,
		.rcpt = NULL,
		.envelopefrom = NULL,
		.length = 0,
		.data = NULL
	};

	if(data_valid){
		if(mail->rcpt){
			free(mail->rcpt);
		}

		if(mail->envelopefrom){
			free(mail->envelopefrom);
		}

		if(mail->data){
			free(mail->data);
		}
	}

	*mail=empty_mail;
	return 0;
}
