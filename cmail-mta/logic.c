int logic_handle_remote(LOGGER log, DATABASE* database, MTA_SETTINGS settings, REMOTE remote){
	int status, delivered_mails=-1;
	unsigned i=0, mx_count=0, port;
	sqlite3_stmt* data_statement=(remote.mode==DELIVER_DOMAIN)?database->query_domain:database->query_remote;

	CONNDATA conn_data = {
	};

	CONNECTION conn = {
		.aux_data = &conn_data
	};

	connection_reset(&conn, true);

	adns_state resolver=NULL;
	adns_answer* resolver_answer=NULL;
	char* mail_remote=remote.host;

	unsigned tx_allocated=0, tx_active=0;
	MAIL* mails=NULL;

	if(!remote.host || strlen(remote.host)<2){
		logprintf(log, LOG_ERROR, "No valid hostname provided\n");
		return -1;
	}

	logprintf(log, LOG_INFO, "Entering mail delivery procedure for host %s in mode %s\n", remote.host, (remote.mode == DELIVER_DOMAIN) ? "domain":"handoff");

	//prepare mail data query
	if(sqlite3_bind_text(data_statement, 1, remote.host, -1, SQLITE_STATIC) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind host parameter\n");
		return -1;
	}

	//read all mail transactions for this host
	do{
		status = sqlite3_step(data_statement);
		switch(status){
			case SQLITE_ROW:
				if(tx_active>=tx_allocated){
					//reallocate transaction array
					mails = realloc(mails, (tx_allocated+CMAIL_REALLOC_CHUNK) * sizeof(MAIL));
					if(!mails){
						logprintf(log, LOG_ERROR, "Failed to allocate memory for mail transaction array\n");
						return -1;
					}
					
					//clear freshly allocated mails
					for(i=0;i<CMAIL_REALLOC_CHUNK;i++){
						mail_reset(mail+i, false);
					}
					
					tx_allocated += CMAIL_REALLOC_CHUNK;
				}
				
				//read transaction
				//TODO	
				
				break;
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "Connection handles %d transactions\n", tx_active);
				break;
			default:
				logprintf(log, LOG_WARNING, "Unhandled response from mail transaction query: %s\n", sqlite3_errmsg(database->conn));
				break;
		}
	}
	while(status == SQLITE_ROW);
	sqlite3_reset(data_statement);
	sqlite3_clear_bindings(data_statement);

	//resolve host for MX records
	if(remote.mode == DELIVER_DOMAIN){
		status = adns_init(&resolver, adns_if_none, log.stream);
		if(status != 0){
			logprintf(log, LOG_ERROR, "Failed to initialize adns: %s\n", strerror(status));
			return -1;
		}

		status = adns_synchronous(resolver, remote.host, adns_r_mx, adns_qf_cname_loose, &resolver_answer);
		if(status != 0){
			logprintf(log, LOG_ERROR, "Failed to run query: %s\n", strerror(status));
			return -1;
		}

		logprintf(log, LOG_DEBUG, "%d records in DNS response\n", resolver_answer->nrrs);

		if(resolver_answer->nrrs<=0){
			logprintf(log, LOG_ERROR, "No MX records for domain %s found, falling back to HANDOFF strategy\n", remote.host);

			//free resolver data
			free(resolver_answer);
			adns_finish(resolver);

			remote.mode=DELIVER_HANDOFF;
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
		if(remote.mode==DELIVER_DOMAIN){
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
	logprintf(log, LOG_INFO, "Finished delivery handling of host %s\n", remote.host);


	if(delivered_mails<0){
		//TODO handle "no mxes reachable" -> increase retry count for all mails
		logprintf(log, LOG_WARNING, "Could not reach any MX for %s\n", remote.host);
	}

	if(remote.mode==DELIVER_DOMAIN){
		free(resolver_answer);
		adns_finish(resolver);
	}

	//free mail transactions
	for(i=0;i<tx_active;i++){
		mail_reset(mails+i, true);
	}
	free(mails);

	connection_reset(&conn, false);
	logprintf(log, LOG_INFO, "Mail delivery for %s done\n", remote.host);
	return delivered_mails;
}

int logic_loop_hosts(LOGGER log, DATABASE* database, MTA_SETTINGS settings){
	int status;
	unsigned i;
	unsigned mails_delivered = 0;
	
	unsigned remotes_allocated = 0;
	unsigned remotes_active = 0;
	REMOTE* remotes = NULL;

	logprintf(log, LOG_INFO, "Entering core loop\n");

	do{
		mails_delivered = 0;
		remotes_active = 0;
	
		//fetch all hosts to be delivered to
		do{
			status=sqlite3_step(database->query_outbound_hosts);
			switch(status){
				case SQLITE_ROW:
					if(remotes_active>=remotes_allocated){
						//reallocate remote array
						remotes=realloc(remotes, (remotes_allocated+CMAIL_REALLOC_CHUNK)*sizeof(REMOTE));
						if(!remotes){
							logprintf(log, LOG_ERROR, "Failed to allocate memory for remote array\n");
							return -1;
						}
						//clear memory
						memset(remotes+remotes_allocated, 0, CMAIL_REALLOC_CHUNK*sizeof(REMOTE));
						remotes_allocated+=CMAIL_REALLOC_CHUNK;
					}

					if(remotes[remotes_active].host){
						free(remotes[remotes_active].host);
					}
					
					remotes[remotes_active].mode=DELIVER_HANDOFF;
					
					if(sqlite3_column_text(database->query_outbound_hosts, 0)){
						remotes[remotes_active].host=common_strdup((char*)sqlite3_column_text(database->query_outbound_hosts, 0));
					}
					else if(sqlite3_column_text(database->query_outbound_hosts, 1)){
						remotes[remotes_active].mode=DELIVER_DOMAIN;
						remotes[remotes_active].host=common_strdup((char*)sqlite3_column_text(database->query_outbound_hosts, 1));
					}

					if(!remotes[remotes_active].host){
						logprintf(log, LOG_ERROR, "Failed to allocate memory for remote host part\n");
						continue;
					}

					remotes_active++;
					break;
				case SQLITE_DONE:
					logprintf(log, LOG_INFO, "Interval contains %d remotes\n", remotes_active);
					break;
				default:
					logprintf(log, LOG_WARNING, "Unhandled outbound host query result: %s\n", sqlite3_errmsg(database->conn));
					break;
			}
		}
		while(status==SQLITE_ROW);
		sqlite3_reset(database->query_outbound_hosts);

		//deliver all remotes
		for(i=0;i<remotes_active;i++){
			logprintf(log, LOG_INFO, "Starting delivery for %s in mode %s\n", remotes[i].host, (remotes[i].mode == DELIVER_DOMAIN) ? "domain":"handoff");
			
			//TODO implement multi-threading here
			status = logic_handle_remote(log, database, settings, remotes[i]);

			if(status<0){
				logprintf(log, LOG_WARNING, "Delivery procedure returned an error\n");
			}
			else{
				mails_delivered += status;
			}
		}
		
		logprintf(log, LOG_INFO, "Core interval done, delivered %d mails\n", mails_delivered);
		
		sleep(settings.check_interval);
	}
	while(!abort_signaled);

	//free allocated remotes
	for(i=0;i<remotes_allocated;i++){
		if(remotes[i].host){
			free(remotes[i].host);
		}
	}
	free(remotes);

	logprintf(log, LOG_INFO, "Core loop exiting cleanly\n");
	return 0;
}
