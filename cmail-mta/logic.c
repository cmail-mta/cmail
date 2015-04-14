int logic_deliver_host(LOGGER log, DATABASE* database, MTA_SETTINGS settings, char* host, DELIVERY_MODE mode){
	int status;
	unsigned delivered_mails=0, i=0, mx_count=0;
	sqlite3_stmt* data_statement=(mode==DELIVER_DOMAIN)?database->query_domain:database->query_remote;
	MAIL current_mail = {
		.ids = NULL,
		.remote = NULL,
		.mailhost = NULL,
		.recipients = NULL,
		.length = 0,
		.data = NULL
	};
	adns_state resolver;
	adns_answer* resolver_answer=NULL;
	char* mail_remote=host;

	if(!host || strlen(host)<2){
		logprintf(log, LOG_ERROR, "No valid hostname provided\n");
		return 0; //TODO return error
	}

	logprintf(log, LOG_INFO, "Entering mail delivery loop for host %s in mode %s\n", host, (mode==DELIVER_DOMAIN)?"domain":"handoff");

	//resolve host for MX records
	if(mode==DELIVER_DOMAIN){
		status=adns_init(&resolver, adns_if_none, log.stream);
		if(status!=0){
			logprintf(log, LOG_ERROR, "Failed to initialize adns: %s\n", strerror(status));
			return 0; //TODO return error
		}

		status=adns_synchronous(resolver, host, adns_r_mx, adns_qf_cname_loose, &resolver_answer);
		if(status!=0){
			logprintf(log, LOG_ERROR, "Failed to run query: %s\n", strerror(status));
			return 0; //TODO return error
		}

		logprintf(log, LOG_DEBUG, "%d records in DNS response\n", resolver_answer->nrrs);

		if(resolver_answer->nrrs<=0){
			logprintf(log, LOG_ERROR, "No MX records for domain %s found, falling back to REMOTE strategy\n", host);
			mode=DELIVER_HANDOFF;
			//TODO check for errors
		}
		else{
			for(i=0;i<resolver_answer->nrrs;i++){
				logprintf(log, LOG_DEBUG, "MX %d: %s\n", i, resolver_answer->rrs.inthostaddr[i].ha.host);
			}

			mx_count=resolver_answer->nrrs;
		}
	}

	//connect to remote
	i=0;
	do{
		if(mode==DELIVER_DOMAIN){
			mail_remote=resolver_answer->rrs.inthostaddr[i].ha.host;
		}
		logprintf(log, LOG_INFO, "Trying to connect to MX %d: %s\n", i, mail_remote);
		
		
		i++;
	}
	while(i<mx_count);

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
	adns_finish(resolver);

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
