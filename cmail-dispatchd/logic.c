int logic_generate_bounces(LOGGER log, DATABASE* database, MTA_SETTINGS settings){
	char time_buffer[SMTP_HEADER_LINE_MAX];
	char message_id[SMTP_HEADER_LINE_MAX];
	int status, rv = 0;
	unsigned bounces = 0, i;

	unsigned bounce_allocated = 0;
	char* bounce_message = NULL;
	time_t unix_time = time(NULL);

	logprintf(log, LOG_INFO, "Entering bounce handling logic\n");

	//create timestring
	if(common_tprintf("%a, %d %b %Y %T %z", unix_time, time_buffer, sizeof(time_buffer) - 1) < 0){
		logprintf(log, LOG_ERROR, "Failed to get current time, buffer length probably not suitable\n");
		snprintf(time_buffer, sizeof(time_buffer) - 1, "Time failed");
	}

	if(sqlite3_bind_int(database->query_bounce_candidates, 1, settings.mail_retries) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind retry amount parameter %d: %s\n", settings.mail_retries, sqlite3_errmsg(database->conn));
		return -1;
	}

	//query all candidates (FIXME collate by mail/sender in order to catch mult-recipient mails. or dont.)
	do{
		status = sqlite3_step(database->query_bounce_candidates);
		switch(status){
			case SQLITE_ROW:
				//ignore bounces of bounces
				if(!sqlite3_column_text(database->query_bounce_candidates, 1)){
					mail_delete(log, database, sqlite3_column_int(database->query_bounce_candidates, 0));
					continue;
				}

				logprintf(log, LOG_DEBUG, "Bouncing message %d from %s retries %d fatality %d\n", sqlite3_column_int(database->query_bounce_candidates, 0), sqlite3_column_text(database->query_bounce_candidates, 1), sqlite3_column_int(database->query_bounce_candidates, 4), sqlite3_column_int(database->query_bounce_candidates, 5));

				//create message id
				snprintf(message_id, sizeof(message_id) - 1, "%X.%X@%s", (unsigned)unix_time, rand(), settings.helo_announce);

				bounce_message = common_strappf(bounce_message, &bounce_allocated,
						"From: %s\r\n" \
						"To: %s\r\n" \
						"Subject: Mail delivery failed, returning message to sender\r\n" \
						"Date: %s\r\n" \
						"Message-Id: <%s>\r\n" \
						"\r\n" \
						"This message was automatically created by the outbound mail delivery system\r\n" \
						"\r\n" \
						"A message you sent (received locally at unixtime %d) could not be\r\n" \
						"delivered to its intended recipient %s by the local system.\r\n" \
						"The following attempts at delivery have been made:\r\n",
						settings.bounce_from,
						sqlite3_column_text(database->query_bounce_candidates, 1),
						time_buffer,
						message_id,
						sqlite3_column_int(database->query_bounce_candidates, 3),
						sqlite3_column_text(database->query_bounce_candidates, 2));
				if(!bounce_message){
					logprintf(log, LOG_ERROR, "Failed to create bounce message base\n");
					break;
				}

				//fetch reasons, append
				if(sqlite3_bind_int(database->query_bounce_reasons, 1, sqlite3_column_int(database->query_bounce_candidates, 0)) != SQLITE_OK){
					logprintf(log, LOG_ERROR, "Failed to bind bounce reason query parameter: %s\n", sqlite3_errmsg(database->conn));
				}

				do{
					status = sqlite3_step(database->query_bounce_reasons);
					switch(status){
						case SQLITE_ROW:
							bounce_message = common_strappf(bounce_message, &bounce_allocated,
									"Unixtime %d %s: %s\r\n",
									sqlite3_column_int(database->query_bounce_reasons, 0),
									(sqlite3_column_int(database->query_bounce_reasons, 2) != 0) ? "(Permanent failure)":"(Temporary failure)",
									sqlite3_column_text(database->query_bounce_reasons, 1));
							if(!bounce_message){
								logprintf(log, LOG_ERROR, "Failed to append bounce reason to bounce message\r\n");
								break;
							}
							break;
						case SQLITE_DONE:
							break;
						default:
							logprintf(log, LOG_ERROR, "Unhandled response to bounce reason query: %s\n", sqlite3_errmsg(database->conn));
							break;
					}
				}
				while(status == SQLITE_ROW);

				sqlite3_reset(database->query_bounce_reasons);
				sqlite3_clear_bindings(database->query_bounce_reasons);

				//append mail data indicator
				bounce_message = common_strappf(bounce_message, &bounce_allocated,
						"\r\n-------Original Message including headers--------\r\n");
				if(!bounce_message){
					logprintf(log, LOG_ERROR, "Failed to append to bounce message\n");
					break;
				}

				if(sqlite3_bind_text(database->insert_bounce, 2, bounce_message, -1, SQLITE_STATIC) != SQLITE_OK
					|| sqlite3_bind_value(database->insert_bounce, 3, sqlite3_column_value(database->query_bounce_candidates, 6)) != SQLITE_OK){
					logprintf(log, LOG_ERROR, "Failed to bind bounce insertion parameter: %s\n", sqlite3_errmsg(database->conn));
				}
				else{
					sqlite3_bind_value(database->insert_bounce, 1, sqlite3_column_value(database->query_bounce_candidates, 1));

					//iterate over intended bounce recipients
					i = 0;
					do{
						//insert into outbox
						if(sqlite3_step(database->insert_bounce) != SQLITE_DONE){
							logprintf(log, LOG_ERROR, "Failed to insert bounce message\n");
							rv = -1;
						}

						sqlite3_reset(database->insert_bounce);
						if(settings.bounce_to && settings.bounce_to[i]){
							if(sqlite3_bind_text(database->insert_bounce, 1, settings.bounce_to[i], -1, SQLITE_STATIC) != SQLITE_OK){
								logprintf(log, LOG_ERROR, "Failed to bind additional bounce recipient: %s\n", sqlite3_errmsg(database->conn));
								rv = -1;
							}
						}
					}
					while(rv == 0 && settings.bounce_to && settings.bounce_to[i++]);

					if(rv == 0){
						//delete original message
						mail_delete(log, database, sqlite3_column_int(database->query_bounce_candidates, 0));

						bounces++;
					}
				}

				sqlite3_reset(database->insert_bounce);
				sqlite3_clear_bindings(database->insert_bounce);

				//reset condition to continue
				status = SQLITE_ROW;
				break;
			case SQLITE_DONE:
				logprintf(log, LOG_INFO, "Generated %d bounce messages\n", bounces);
				break;
			default:
				logprintf(log, LOG_WARNING, "Unhandled database response in bounce generation: %s\n", sqlite3_errmsg(database->conn));
				rv = -1;
				break;
		}
	}
	while(status == SQLITE_ROW);

	sqlite3_reset(database->query_bounce_candidates);
	sqlite3_clear_bindings(database->query_bounce_candidates);

	if(bounce_message){
		free(bounce_message);
	}

	return (rv < 0) ? rv:bounces;
}

int logic_handle_transaction(LOGGER log, DATABASE* database, CONNECTION* conn, MAIL* transaction){
	int delivered_mails = 0;
	unsigned i;
	CONNDATA* conn_data = (CONNDATA*) conn->aux_data;

	if(mail_dispatch(log, database, transaction, conn) < 0){
		logprintf(log, LOG_WARNING, "Failed to dispatch transaction\n");
		//need to reset transaction here because greylisting may fail the first connection,
		//but we'd still like to try the next
		smtp_rset(log, conn);
		return -1;
	}

	//reset in case any transaction follows
	smtp_rset(log, conn);

	//handle failcount increase
	for(i = 0; i < transaction->recipients; i++){
		switch(transaction->rcpt[i].status){
			case RCPT_OK:
				//delivery done, remove from database
				if(mail_delete(log, database, transaction->rcpt[i].dbid) < 0){
					logprintf(log, LOG_WARNING, "Failed to delete delivered mail id %d\n", transaction->rcpt[i].dbid);
				}

				delivered_mails++;
				break;
			case RCPT_FAIL_TEMPORARY:
			case RCPT_FAIL_PERMANENT:
				//handled by bouncehandler
				break;
			case RCPT_READY:
				logprintf(log, LOG_WARNING, "Recipient %d not touched by dispatch loop\n", i);
				//this happens if the peer rejects the MAIL command
				mail_failure(log, database, transaction->rcpt[i].dbid, conn_data->reply.response_text, false);
				break;
		}
	}


	logprintf(log, LOG_INFO, "Handled %d mails in transaction\n", delivered_mails);
	return delivered_mails;
}

int logic_handle_remote(LOGGER log, DATABASE* database, MTA_SETTINGS settings, REMOTE remote){
	int status, delivered_mails = -1;
	unsigned current_mx = 0, mx_count = 0, port, i, c;
	char* failure_reason = "Failed to reach any MX";
	bool fail_permanently = false;
	sqlite3_stmt* tx_statement = (remote.mode == DELIVER_DOMAIN) ? database->query_domain:database->query_remote;

	CONNDATA conn_data = {
	};

	CONNECTION conn = {
		.aux_data = &conn_data
	};

	connection_reset(log, &conn, false);

	adns_state resolver = NULL;
	adns_answer* resolver_answer = NULL;
	char* resolver_remote = NULL;

	unsigned tx_allocated = 0, tx_active = 0;
	MAIL* mails = NULL;

	if(!remote.host || strlen(remote.host) < 2){
		logprintf(log, LOG_ERROR, "No valid hostname provided\n");
		return -1;
	}

	logprintf(log, LOG_INFO, "Entering mail remote handling for host %s in mode %s\n", remote.host, (remote.mode == DELIVER_DOMAIN) ? "domain":"handoff");

	//resolve host for MX records
	resolver_remote = remote.host;
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

		if(resolver_answer->status == adns_s_ok && resolver_answer->nrrs <= 0){
			logprintf(log, LOG_ERROR, "No MX records for domain %s found, falling back to HANDOFF strategy\n", remote.host);

			//free resolver data
			free(resolver_answer);
			adns_finish(resolver);

			remote.mode = DELIVER_HANDOFF;
			mx_count = 1;
			//TODO report error type
		}
		else if(resolver_answer->status == adns_s_ok){
			for(c = 0; c < resolver_answer->nrrs; c++){
				logprintf(log, LOG_DEBUG, "MX %d: %s\n", c, resolver_answer->rrs.inthostaddr[c].ha.host);
			}

			mx_count = resolver_answer->nrrs;
		}
		else{
			//recoverable failures should be retried, permanent errors should fail all mails
			//in the transaction
			//fail
			//	adns_s_nxdomain
			//	adns_s_nodata(?)
			//this is checked again below
			if(resolver_answer->status == adns_s_nxdomain){
				logprintf(log, LOG_ERROR, "DNS query returned an irrecoverable error, failing this remote permanently\n");
			}
			//retry
			//	the rest
			else{
				logprintf(log, LOG_ERROR, "DNS query returned a recoverable error, retrying this remote in the next interval\n");
				free(resolver_answer);
				adns_finish(resolver);
				return -1;
			}
		}
	}
	else{
		mx_count = 1;
	}

	//prepare mail data query
	if(sqlite3_bind_int(tx_statement, 1, settings.retry_interval) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind timeout parameter\n");
		//FIXME returning -1 here retries in the next interval, leaking adns_*
		return -1;
	}

	if(sqlite3_bind_text(tx_statement, 2, remote.remotespec, -1, SQLITE_STATIC) != SQLITE_OK){
		logprintf(log, LOG_ERROR, "Failed to bind host parameter\n");
		//FIXME returning -1 here retries in the next interval, leaking adns_*
		return -1;
	}

	//read all mail transactions for this host
	do{
		status = sqlite3_step(tx_statement);
		switch(status){
			case SQLITE_ROW:
				if(tx_active >= tx_allocated){
					//reallocate transaction array
					mails = realloc(mails, (tx_allocated + CMAIL_REALLOC_CHUNK) * sizeof(MAIL));
					if(!mails){
						logprintf(log, LOG_ERROR, "Failed to allocate memory for mail transaction array\n");
						//FIXME returning -1 here retries in the next interval, leaking adns_*
						return -1;
					}

					//clear freshly allocated mails
					for(c = 0; c < CMAIL_REALLOC_CHUNK; c++){
						mail_reset(mails + c, false);
					}

					tx_allocated += CMAIL_REALLOC_CHUNK;
				}

				//read transaction
				if(mail_dbread(log, mails + tx_active, tx_statement) < 0){
					logprintf(log, LOG_ERROR, "Failed to read transaction %s, database status %s\n", (char*)sqlite3_column_text(tx_statement, 0), sqlite3_errmsg(database->conn));
				}

				tx_active++;
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
	sqlite3_reset(tx_statement);
	sqlite3_clear_bindings(tx_statement);

	//connect to remote
	for(current_mx = 0; current_mx < mx_count; current_mx++){
		if(remote.mode == DELIVER_DOMAIN){
			resolver_remote = resolver_answer->rrs.inthostaddr[current_mx].ha.host;
		}
		logprintf(log, LOG_INFO, "Trying to connect to MX %d: %s\n", current_mx, resolver_remote);

		for(port = 0; settings.port_list[port].port; port++){
			REMOTE_PORT current_port = settings.port_list[port];

			if(remote.mode == DELIVER_HANDOFF && remote.forced_port.port){
				//need to break out of port iteration again at end
				logprintf(log, LOG_INFO, "Using forced port for handoff remote\n");
				current_port = remote.forced_port;
			}

			#ifndef CMAIL_NO_TLS
			logprintf(log, LOG_INFO, "Trying port %d TLS mode %s\n", current_port.port, tls_modestring(current_port.tls_mode));
			#else
			logprintf(log, LOG_INFO, "Trying port %d\n", current_port.port);
			#endif

			conn.fd = network_connect(log, resolver_remote, current_port.port);
			//TODO only reconnect if port or remote have changed

			if(conn.fd > 0){
				//negotiate smtp
				if(smtp_negotiate(log, settings, resolver_remote, &conn, current_port, remote.remote_auth) < 0){
					logprintf(log, LOG_INFO, "Failed to negotiate required protocol level, trying next\n");
					//FIXME might want to gracefully close smtp here
					connection_reset(log, &conn, true);
					//if forced port, dont try any others
					if(remote.mode == DELIVER_HANDOFF && remote.forced_port.port){
						break;
					}
					continue;
				}

				//connected, run the delivery loop
				delivered_mails = 0;
				for(c = 0; c < tx_active; c++){
					status = logic_handle_transaction(log, database, &conn, mails + c);
					if(status < 0){
						logprintf(log, LOG_WARNING, "Mail transaction failed, continuing\n");
					}
					else{
						delivered_mails += status;
					}
				}

				logprintf(log, LOG_INFO, "Delivered %d mails in %d transactions for this remote\n", delivered_mails, tx_active);

				//FIXME might want to continue if not all mail could be delivered
				current_mx = mx_count; //break the outer loop
				break;
			}

			//if forced port, dont try any others
			if(remote.mode == DELIVER_HANDOFF && remote.forced_port.port){
				break;
			}
		}
	}
	logprintf(log, LOG_INFO, "Finished delivery handling of host %s after trying %d of %d MXes\n", remote.host, current_mx, mx_count);

	if(delivered_mails < 0){
		//FIXME this does not account for partial delivery by some mailservers - or does it?
		//TODO check who actually deletes completed mails - are all nondeleted mails retried?
		if(remote.mode == DELIVER_DOMAIN && 
				(resolver_answer->status == adns_s_nxdomain)){
			//permanently fail the mails
			logprintf(log, LOG_WARNING, "Invalid remotespec %s, permanently failing\n", remote.host);
			failure_reason = "Failed to resolve recipient domain";
			fail_permanently = true;
		}
		else if(remote.mode == DELIVER_HANDOFF){
			logprintf(log, LOG_WARNING, "Connection or TLS negotiation failed with handoff remote %s\n", remote.host);
			failure_reason = "Failed to connect or negotiate with handoff remote";
		}
		else{
			logprintf(log, LOG_WARNING, "Could not reach any MX for %s, failing temporarily\n", remote.host);
		}
		
		//insert failure reasons
		for(i = 0; i < tx_active; i++){
			for(c = 0; c < mails[i].recipients; c++){
				mail_failure(log, database, mails[i].rcpt[c].dbid, failure_reason, fail_permanently);
			}
		}
	}

	if(remote.mode == DELIVER_DOMAIN){
		free(resolver_answer);
		adns_finish(resolver);
	}

	//free mail transactions
	for(c = 0; c < tx_active; c++){
		mail_reset(mails + c, true);
	}
	free(mails);

	connection_reset(log, &conn, true);
	logprintf(log, LOG_INFO, "Mail delivery for %s done\n", remote.host);
	return delivered_mails;
}

int logic_loop_hosts(LOGGER log, DATABASE* database, MTA_SETTINGS settings){
	int status;
	unsigned u;
	unsigned mails_delivered = 0;
	int bounces_generated = 0;

	unsigned remotes_allocated = 0;
	unsigned remotes_active = 0;
	REMOTE* remotes = NULL;
	char* remote_separator;

	logprintf(log, LOG_INFO, "Entering core loop\n");

	do{
		mails_delivered = 0;
		remotes_active = 0;

		//bind the timeout parameter
		if(sqlite3_bind_int(database->query_outbound_hosts, 1, settings.retry_interval) != SQLITE_OK){
			logprintf(log, LOG_ERROR, "Failed to bind timeout parameter\n");
			break;
		}

		//fetch all hosts to be delivered to
		do{
			status = sqlite3_step(database->query_outbound_hosts);
			switch(status){
				case SQLITE_ROW:
					if(remotes_active >= remotes_allocated){
						//reallocate remote array
						remotes = realloc(remotes, (remotes_allocated + CMAIL_REALLOC_CHUNK) * sizeof(REMOTE));
						if(!remotes){
							logprintf(log, LOG_ERROR, "Failed to allocate memory for remote array\n");
							return -1;
						}
						//clear memory
						memset(remotes + remotes_allocated, 0, CMAIL_REALLOC_CHUNK * sizeof(REMOTE));
						remotes_allocated += CMAIL_REALLOC_CHUNK;
					}

					//reset entry
					if(remotes[remotes_active].host){
						free(remotes[remotes_active].host);
					}
					if(remotes[remotes_active].remotespec){
						free(remotes[remotes_active].remotespec);
					}
					if(remotes[remotes_active].remote_auth){
						free(remotes[remotes_active].remote_auth);
						remotes[remotes_active].remote_auth = NULL;
					}
					remotes[remotes_active].forced_port.port = 0;
					remotes[remotes_active].forced_port.tls_mode = TLS_NONE;
					remotes[remotes_active].mode = DELIVER_HANDOFF;

					if(sqlite3_column_text(database->query_outbound_hosts, 0)){
						remotes[remotes_active].remotespec = common_strdup((char*)sqlite3_column_text(database->query_outbound_hosts, 0));
						remotes[remotes_active].host = common_strdup(remotes[remotes_active].remotespec);
						//parse additional handoff remote parameters
						//check for remote authentication
						if(index(remotes[remotes_active].host, '@')){
							//copy remote authentication raw
							remote_separator = index(remotes[remotes_active].host, '@');
							*remote_separator = 0;
							remotes[remotes_active].remote_auth = strdup(remotes[remotes_active].host);
							//FIXME error checking

							//move host part back to .host
							memmove(remotes[remotes_active].host, remote_separator + 1, strlen(remote_separator + 1) + 1);

							//preprocess SASL response
							u = strlen(remotes[remotes_active].remote_auth);
							remotes[remotes_active].remote_auth = realloc(remotes[remotes_active].remote_auth, (u + 2) * sizeof(char));
							memmove(remotes[remotes_active].remote_auth + 1, remotes[remotes_active].remote_auth, u + 1);
							remote_separator = index(remotes[remotes_active].remote_auth, ':');
							if(!remote_separator){
								//TODO fail here
							}
							else{
								*remote_separator = 0;
							}
							*remotes[remotes_active].remote_auth = 0;
							if(auth_base64encode(log, (uint8_t**)&(remotes[remotes_active].remote_auth), u + 2) < 0){
								//TODO fail here
							}
						}
						//check for port specification (and tls mode)
						if(index(remotes[remotes_active].host, ':')){
							remote_separator = index(remotes[remotes_active].host, ':');
							*remote_separator = 0;

							//parse remote port
							remotes[remotes_active].forced_port.port = strtoul(remote_separator + 1, &remote_separator, 0);
							//check for tls modestring immediately following
							if(*remote_separator == '/'){
								if(!strcasecmp(remote_separator, "/starttls")){
									remotes[remotes_active].forced_port.tls_mode = TLS_NEGOTIATE;
								}
								else if(!strcasecmp(remote_separator, "/tls")){
									remotes[remotes_active].forced_port.tls_mode = TLS_ONLY;
								}
								else{
									remotes[remotes_active].forced_port.tls_mode = TLS_NONE;
									//FIXME might want to fail the remote upon invalid TLSMODE
								}
							}
							else if(*remote_separator != 0){
								//invalid character
								//TODO fail this remote?
							}
						}
					}
					else if(sqlite3_column_text(database->query_outbound_hosts, 1)){
						remotes[remotes_active].mode = DELIVER_DOMAIN;
						remotes[remotes_active].host = common_strdup((char*)sqlite3_column_text(database->query_outbound_hosts, 1));
						remotes[remotes_active].remotespec = common_strdup(remotes[remotes_active].host);
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
		while(status == SQLITE_ROW);
		sqlite3_reset(database->query_outbound_hosts);
		sqlite3_clear_bindings(database->query_outbound_hosts);

		//deliver all remotes
		for(u = 0; u < remotes_active; u++){
			logprintf(log, LOG_INFO, "Starting delivery for %s in mode %s\n", remotes[u].host, (remotes[u].mode == DELIVER_DOMAIN) ? "domain":"handoff");
			if(remotes[u].forced_port.port){
				logprintf(log, LOG_INFO, "Remote uses forced port %d with TLS mode %s\n", remotes[u].forced_port.port, tls_modestring(remotes[u].forced_port.tls_mode));
			}
			if(remotes[u].remote_auth){
				logprintf(log, LOG_INFO, "Remote uses remote authentication\n");
				logprintf(log, LOG_DEBUG, "Remote authentication data %s\n", remotes[u].remote_auth);
			}

			//TODO implement multi-threading here
			status = logic_handle_remote(log, database, settings, remotes[u]);

			if(status < 0){
				logprintf(log, LOG_WARNING, "Delivery procedure returned an error\n");
			}
			else{
				mails_delivered += status;
			}
		}

		bounces_generated = logic_generate_bounces(log, database, settings);
		if(bounces_generated < 0){
			logprintf(log, LOG_WARNING, "Bounce generation reported an error\n");
			bounces_generated = 0;
		}

		logprintf(log, LOG_INFO, "Core interval done, delivered %d mails, generated %d bounces\n", mails_delivered, bounces_generated);

		sleep(settings.check_interval);
	}
	while(!abort_signaled);

	//free allocated remotes
	for(u = 0; u < remotes_allocated; u++){
		if(remotes[u].host){
			free(remotes[u].host);
		}
		if(remotes[u].remotespec){
			free(remotes[u].remotespec);
		}
		if(remotes[u].remote_auth){
			free(remotes[u].remote_auth);
		}
	}
	free(remotes);

	logprintf(log, LOG_INFO, "Core loop exiting cleanly\n");
	return 0;
}
