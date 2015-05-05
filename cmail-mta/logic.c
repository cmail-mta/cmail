int logic_loop_proto(LOGGER log, DATABASE* database, MTA_SETTINGS settings, char* host, DELIVERY_MODE mode){
	int status, delivered_mails=-1;
	unsigned i=0, mx_count=0, port;
	sqlite3_stmt* data_statement=(mode==DELIVER_DOMAIN)?database->query_domain:database->query_remote;

	CONNDATA conn_data = {
	};

	CONNECTION conn = {
		.aux_data = &conn_data
	};

	connection_reset(&conn, true);

	adns_state resolver=NULL;
	adns_answer* resolver_answer=NULL;
	char* mail_remote=host;

	if(!host || strlen(host)<2){
		logprintf(log, LOG_ERROR, "No valid hostname provided\n");
		return -1;
	}

	logprintf(log, LOG_INFO, "Entering mail delivery procedure for host %s in mode %s\n", host, (mode==DELIVER_DOMAIN)?"domain":"handoff");

	//prepare mail data query
	if(sqlite3_bind_text(data_statement, 1, host, -1, SQLITE_STATIC)!=SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind host parameter\n");
		return -1;
	}

	//resolve host for MX records
	if(mode==DELIVER_DOMAIN){
		status=adns_init(&resolver, adns_if_none, log.stream);
		if(status!=0){
			logprintf(log, LOG_ERROR, "Failed to initialize adns: %s\n", strerror(status));
			return -1;
		}

		status=adns_synchronous(resolver, host, adns_r_mx, adns_qf_cname_loose, &resolver_answer);
		if(status!=0){
			logprintf(log, LOG_ERROR, "Failed to run query: %s\n", strerror(status));
			return -1;
		}

		logprintf(log, LOG_DEBUG, "%d records in DNS response\n", resolver_answer->nrrs);

		if(resolver_answer->nrrs<=0){
			logprintf(log, LOG_ERROR, "No MX records for domain %s found, falling back to HANDOFF strategy\n", host);

			//free resolver data
			free(resolver_answer);
			adns_finish(resolver);

			mode=DELIVER_HANDOFF;
			//TODO report error type
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

		for(port=0;settings.port_list[port].port;port++){
			#ifndef CMAIL_NO_TLS
			logprintf(log, LOG_INFO, "Trying port %d TLS mode %s\n", settings.port_list[port].port, tls_modestring(settings.port_list[port].tls_mode));
			#else
			logprintf(log, LOG_INFO, "Trying port %d\n", settings.port_list[port].port);
			#endif

			conn.fd=network_connect(log, mail_remote, settings.port_list[port].port);
			//TODO only reconnect if port or remote have changed

			if(conn.fd>0){
				//negotiate smtp
				if(smtp_negotiate(log, settings, mail_remote, &conn, settings.port_list[port])<0){
					logprintf(log, LOG_INFO, "Failed to negotiate required protocol level, trying next\n");
					//FIXME might want to gracefully close smtp here
					connection_reset(&conn, false);
					continue;
				}

				//connected, run the delivery loop
				delivered_mails=smtp_deliver_loop(log, database, data_statement, &conn);
				if(delivered_mails<0){
					logprintf(log, LOG_WARNING, "Failed to deliver mail\n");
					//failed to deliver != no mx reachable
					delivered_mails=0;
				}
				//FIXME might want to continue if not all mail could be delivered
				i=mx_count; //break the outer loop
				break;
			}
		}

		i++;
	}
	while(i<mx_count);


	if(delivered_mails<0){
		//TODO handle "no mxes reachable" -> increase retry count for all mails
		logprintf(log, LOG_WARNING, "Could not reach any MX for %s\n", host);
	}

	if(mode==DELIVER_DOMAIN){
		free(resolver_answer);
		adns_finish(resolver);
	}

	connection_reset(&conn, false);
	logprintf(log, LOG_INFO, "Mail delivery for %s done\n", host);
	return delivered_mails;
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
					status=logic_loop_proto(log, database, settings, mail_remote, mail_mode);

					if(status<0){
						logprintf(log, LOG_WARNING, "Delivery procedure returned an error\n");
					}
					else{
						mails_delivered+=status;
					}

					status=SQLITE_ROW;

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
