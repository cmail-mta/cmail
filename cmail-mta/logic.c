int logic_deliver_host(LOGGER log, DATABASE* database, MTA_SETTINGS settings, char* host){
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

	logprintf(log, LOG_INFO, "Entering mail delivery loop\n");
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
	char* mail_remote=NULL;

	logprintf(log, LOG_INFO, "Entering core loop\n");

	do{
		do{
			status=sqlite3_step(database->query_outbound_hosts);
			switch(status){
				case SQLITE_ROW:
					mail_remote=(char*)sqlite3_column_text(database->query_outbound_hosts, 0);
					if(!mail_remote){
						mail_remote=(char*)sqlite3_column_text(database->query_outbound_hosts, 1);
					}
					//handle outbound mail for single host
					logprintf(log, LOG_INFO, "Starting delivery for %s\n", mail_remote);
					//TODO actually start delivery
					break;
				case SQLITE_DONE:
					//TODO count mails
					logprintf(log, LOG_INFO, "Interval done\n");
					break;
			}
		}
		while(status==SQLITE_ROW);
	
		sqlite3_reset(database->query_outbound_hosts);
		sleep(settings.check_interval);
	}
	while(!abort_signaled);
	
	logprintf(log, LOG_INFO, "Core loop exiting cleanly\n");
	return 0;
}
