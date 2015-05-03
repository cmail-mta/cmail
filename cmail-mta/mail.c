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

int mail_dispatch(LOGGER log, DATABASE* database, MAIL* mail, CONNECTION* conn){
	unsigned i;
	
	if(smtp_initiate(log, conn, mail)<0){
		logprintf(log, LOG_WARNING, "Failed to initiate mail transaction\n");
		//TODO mark mails not sent
		return -1;
	}

	for(i=0;i<mail->recipients;i++){
		sqlite3_bind_int(database->query_rcpt, 1, mail->rcpt[i].dbid);
		switch(sqlite3_step(database->query_rcpt)){
			case SQLITE_DONE:
				logprintf(log, LOG_WARNING, "dbid %d does not exist anymore\n", mail->rcpt[i].dbid);
				mail->rcpt[i].status=RCPT_FAIL_PERMANENT;
				break;
			case SQLITE_ROW:
				switch(smtp_rcpt(log, conn, (char*)sqlite3_column_text(database->query_rcpt, 0))){
					case -1:
						logprintf(log, LOG_INFO, "Recipient %d: %s failed permanently\n", i, (char*)sqlite3_column_text(database->query_rcpt, 0));
						mail->rcpt[i].status=RCPT_FAIL_PERMANENT;
						break;
					case 0:
						logprintf(log, LOG_INFO, "Recipient %d: %s accepted\n", i, (char*)sqlite3_column_text(database->query_rcpt, 0));
						mail->rcpt[i].status=RCPT_OK;
						break;
					case 1:
						logprintf(log, LOG_INFO, "Recipient %d: %s failed temporarily\n", i, (char*)sqlite3_column_text(database->query_rcpt, 0));
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

	switch(smtp_data(log, conn, mail->data)){
		case -1:
			//set all recipients to permanent fail
			logprintf(log, LOG_WARNING, "Mail rejected\n");
			for(i=0;i<mail->recipients;i++){
				mail->rcpt[i].status=RCPT_FAIL_PERMANENT;
			}
			return -1;
		case 1:
			//set all recipients to temp fail
			for(i=0;i<mail->recipients;i++){
				mail->rcpt[i].status=RCPT_FAIL_TEMPORARY;
			}
			logprintf(log, LOG_WARNING, "Mail not accepted, deferring\n");
			return -1;
		case 0:
			logprintf(log, LOG_INFO, "Mail accepted\n");
			smtp_rset(log, conn);
			return 0;
	}

	return -1;
}

int mail_free(MAIL* mail){
	MAIL empty_mail = {
		.recipients = 0,
		.rcpt = NULL,
		.envelopefrom = NULL,
		.length = 0,
		.data = NULL
	};

	if(mail->rcpt){
		free(mail->rcpt);
	}

	if(mail->envelopefrom){
		free(mail->envelopefrom);
	}

	if(mail->data){
		free(mail->data);
	}

	*mail=empty_mail;
	return 0;
}
