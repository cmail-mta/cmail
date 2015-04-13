int logic_deliver_host(LOGGER log, DATABASE* database, MTA_SETTINGS settings, char* host, DELIVERY_MODE mode){
	int status;
	unsigned delivered_mails=0, i;
	sqlite3_stmt* data_statement=(mode==DELIVER_DOMAIN)?database->query_domain:database->query_remote;
	MAIL current_mail = {
		.ids = NULL,
		.remote = NULL,
		.mailhost = NULL,
		.recipients = NULL,
		.length = 0,
		.data = NULL
	};
	unsigned char nsbuf[4096]; //FIXME this is currently hardcoded
	ns_msg msg;
	ns_rr rr;

	if(!host || strlen(host)<2){
		logprintf(log, LOG_ERROR, "No valid hostname provided\n");
		return 0; //TODO return error
	}

	logprintf(log, LOG_INFO, "Entering mail delivery loop for host %s in mode %s\n", host, (mode==DELIVER_DOMAIN)?"domain":"handoff");

	//TODO resolve mail host
	status=res_query(host, ns_c_any, ns_t_mx, nsbuf, sizeof(nsbuf));
	if(status<0){
		logprintf(log, LOG_ERROR, "Failed to resolve host %s: %s\n", host, strerror(errno));
		return 0; //TODO return error
	}

	logprintf(log, LOG_DEBUG, "%d bytes of nameserver data\n", status);
	ns_initparse(nsbuf, status, &msg);
	status=ns_msg_count(msg, ns_s_an);
	
	for(i=0;i<status;i++){
		ns_parserr(&msg, ns_s_an, i, &rr);
		//logprintf(log, LOG_DEBUG, "MX List entry %d, %d bytes: %s\n", i, ns_rr_rdlen(rr), ns_rr_rdata(rr)+1);
	}

	//set data query
	if(sqlite3_bind_text(data_statement, 1, host, -1, SQLITE_STATIC)!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind host parameter\n");
		return 0; //TODO return error without messing up the sum
	}

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

	logprintf(log, LOG_INFO, "Mail delivery loop for %s done\n", host);
	return 0;
}

int logic_loop_hosts(LOGGER log, DATABASE* database, MTA_SETTINGS settings){
	int status;
	DELIVERY_MODE mail_mode=DELIVER_DOMAIN;
	char* mail_remote=NULL;
	unsigned mails_delivered=0;

	logprintf(log, LOG_INFO, "Entering core loop\n");

	do{
		mails_delivered=0;
		do{
			status=sqlite3_step(database->query_outbound_hosts);
			switch(status){
				case SQLITE_ROW:
					mail_mode=DELIVER_HANDOFF;
					mail_remote=(char*)sqlite3_column_text(database->query_outbound_hosts, 0);
					
					if(!mail_remote){
						mail_mode=DELIVER_DOMAIN;
						mail_remote=(char*)sqlite3_column_text(database->query_outbound_hosts, 1);
					}
					//handle outbound mail for single host
					logprintf(log, LOG_INFO, "Starting delivery for %s in mode %s\n", mail_remote, (mail_mode==DELIVER_DOMAIN)?"domain":"handoff");
					
					//TODO implement multi-threading here
					mails_delivered+=logic_deliver_host(log, database, settings, mail_remote, mail_mode);

					break;
				case SQLITE_DONE:
					logprintf(log, LOG_INFO, "Interval done, delivered %d mails\n", mails_delivered);
					break;
			}
		}
		while(status==SQLITE_ROW);
	
		sqlite3_reset(database->query_outbound_hosts);
	
		//TODO check for mails over the retry limit
		sleep(settings.check_interval);
	}
	while(!abort_signaled);
	
	logprintf(log, LOG_INFO, "Core loop exiting cleanly\n");
	return 0;
}
