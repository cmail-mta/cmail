int logic_deliver_host(LOGGER log, DATABASE* database, MTA_SETTINGS settings, char* host, DELIVERY_MODE mode){
	int status;
	int mails;
	MAIL current_mail = {
		.ids = NULL,
		.remote = NULL,
		.mailhost = NULL,
		.recipients = NULL,
		.length = 0,
		.data = NULL
	};

	logprintf(log, LOG_INFO, "Entering mail delivery loop for host %s in mode %s\n", host, (mode==DELIVER_DOMAIN)?"domain":"handoff");
	//TODO select only mails from one host
/*
	mails=0;
	do{
		status=sqlite3_step(database->query_outbound_mail);
		switch(status){
			case SQLITE_ROW:
				//handle outbound mail
				if(mail_dbread(log, &current_mail, database->query_outbound_mail)<0){
					logprintf(log, LOG_ERROR, "Failed to handle mail with IDlist %s, database status %s\n", (char*)sqlite3_column_text(database->query_outbound_mail, 0), sqlite3_errmsg(database->conn));
				}
				else{
					if(mail_dispatch(log, database, &current_mail)<0){
						logprintf(log, LOG_WARNING, "Failed to dispatch mail with IDlist %s\n", (char*)sqlite3_column_text(database->query_outbound_mail, 0));
					}
					mails++;
				}
				mail_free(&current_mail);
				break;
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "Iteration done, handled %d mails\n", mails);
				break;
		}
	}
	while(status==SQLITE_ROW);

	sqlite3_reset(database->query_outbound_mail);
	sleep(settings.check_interval);
*/	
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
