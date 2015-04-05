int core_loop(LOGGER log, DATABASE* database){
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

	logprintf(log, LOG_INFO, "Entering core loop\n");

	do{
		mails=0;
		do{
			status=sqlite3_step(database->query_outbound);
			switch(status){
				case SQLITE_ROW:
					//handle outbound mail
					if(mail_dbread(log, &current_mail, database->query_outbound)<0){
						logprintf(log, LOG_ERROR, "Failed to handle mail with IDlist %s, database status %s\n", (char*)sqlite3_column_text(database->query_outbound, 0), sqlite3_errmsg(database->conn));
					}
					else{
						if(mail_dispatch(log, database, &current_mail)<0){
							logprintf(log, LOG_WARNING, "Failed to dispatch mail with IDlist %s\n", (char*)sqlite3_column_text(database->query_outbound, 0));
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
	
		sqlite3_reset(database->query_outbound);
		sleep(CMAIL_MTA_INTERVAL);
	}
	while(!abort_signaled);
	
	logprintf(log, LOG_INFO, "Core loop exiting cleanly\n");
	return 0;
}
