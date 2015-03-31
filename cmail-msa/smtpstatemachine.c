/*
 * Transitions
 * 	NEW | HELO/EHLO -> IDLE | 250 CAPS
 * 	NEW | *		-> NEW | 500 Out of sequence
 *	IDLE | MAIL	-> MAIL | 250 OK
 *	IDLE | RSET	-> IDLE | 250 OK
 *	MAIL | RSET	-> IDLE | 250 OK
 */

int smtpstate_new(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;

	if(!strncasecmp(client_data->recv_buffer, "ehlo ", 5)){
		#ifndef CMAIL_NO_TLS
		switch(listener_data->tls_mode){
			case TLS_ONLY:
				client_data->current_mail.protocol="sesmtp";
				break;
			case TLS_NEGOTIATE:
				if(client_data->tls_mode==TLS_ONLY){
					client_data->current_mail.protocol="esmtps";
					break;
				}
			case TLS_NONE:
				client_data->current_mail.protocol="esmtp";
				break;
		}
		#else
		client_data->current_mail.protocol="esmtp";
		#endif

		client_data->state=STATE_IDLE;
		logprintf(log, LOG_INFO, "Client negotiates esmtp\n");

		client_send(log, client, "250-%s ahoyhoy\r\n", listener_data->announce_domain);
		
		//TODO hook plugins here
		
		//send hardcoded esmtp options
		client_send(log, client, "250-SIZE 52428800\r\n"); //FIXME make this configurable
		client_send(log, client, "250-8BITMIME\r\n"); //FIXME this might imply more processing than planned
		client_send(log, client, "250-SMTPUTF8\r\n"); //RFC 6531
		switch(listener_data->auth_offer){
			case AUTH_NONE:
				break;
			case AUTH_TLSONLY:
				#ifndef CMAIL_NO_TLS
				if(client_data->tls_mode!=TLS_ONLY){
					break;
				}
				#else
				break;
				#endif
			case AUTH_ANY:
				client_send(log, client, "250-AUTH PLAIN\r\n");
				break;
		}
		#ifndef CMAIL_NO_TLS
		//advertise only when possible
		if(listener_data->tls_mode==TLS_NEGOTIATE && client_data->tls_mode==TLS_NONE){
			client_send(log, client, "250-STARTTLS\r\n"); //RFC 3207
		}
		#endif
		client_send(log, client, "250 XYZZY\r\n"); //RFC 5321 2.2.2
		return 0;
	}
	
	if(!strncasecmp(client_data->recv_buffer, "helo ", 5)){
		#ifndef CMAIL_NO_TLS
		switch(listener_data->tls_mode){
			case TLS_ONLY:
				client_data->current_mail.protocol="ssmtp";
				break;
			case TLS_NEGOTIATE:
				if(client_data->tls_mode==TLS_ONLY){
					client_data->current_mail.protocol="smtps";
					break;
				}
			case TLS_NONE:
				client_data->current_mail.protocol="smtp";
				break;
		}
		#else
		client_data->current_mail.protocol="smtp";
		#endif

		client_data->state=STATE_IDLE;
		logprintf(log, LOG_INFO, "Client negotiates smtp\n");

		client_send(log, client, "250 %s ahoyhoy\r\n", listener_data->announce_domain);
		return 0;
	}
	
	logprintf(log, LOG_INFO, "Command not recognized in state NEW: %s\n", client_data->recv_buffer);
	client_send(log, client, "500 Unknown command\r\n");
	return -1;		
}

int smtpstate_auth(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;
	char* parameter=client_data->recv_buffer;

	if(!strcmp(client_data->recv_buffer, "*")){
		//cancel authentication
		logprintf(log, LOG_INFO, "Client cancelled authentication\n");
		client_data->state=STATE_IDLE;
		client_send(log, client, "501 Authentication failed\r\n");
		return 0;
	}

	//method select & initial parameter
	if(!strncasecmp(client_data->recv_buffer, "auth ", 5)){
		if(!strncasecmp(client_data->recv_buffer+5, "plain", 5)){
			client_data->auth.method=AUTH_PLAIN;
		}
		//may check for other methods here
		else{
			logprintf(log, LOG_WARNING, "Client tried unsupported authentication method: %s\n", client_data->recv_buffer+5);
			client_data->state=STATE_IDLE;
			client_send(log, client, "504 Unknown mechanism\r\n");
			return 0;
		}

		//scan over method
		for(parameter=client_data->recv_buffer+5;!isspace(parameter[0])&&parameter[0];parameter++){
		}

		if(parameter[0]&&strlen(parameter)>1){
			//parameter supplied, continue
			parameter++;
		}
		else{
			//no parameter, ask for continuation
			switch(client_data->auth.method){
				case AUTH_PLAIN:
					client_send(log, client, "334 \r\n");
					break;
			}
			return 0;
		}
	}
	
	//FIXME refactor this part
	//catch (unprefixed) data parameters
	switch(client_data->auth.method){
		case AUTH_PLAIN:
			//must be parameter.
			//duplicate to storage
			client_data->auth.parameter=common_strdup(parameter);
			if(!client_data->auth.parameter){
				logprintf(log, LOG_ERROR, "Failed to allocate auth parameter memory\n");
				client_data->state=STATE_IDLE;
				client_send(log, client, "454 Internal Error\r\n");
				auth_reset(&(client_data->auth));
				return -1;
			}
			
			//evaluate
			switch(auth_status(log, database, &(client_data->auth))){
				case 1:
					//more info required
					//TODO more info required
				case -1:
					logprintf(log, LOG_INFO, "Client failed to authenticate\n");
					client_send(log, client, "535 Authentication failed\r\n");
					auth_reset(&(client_data->auth));
					break;
				case 0:
					logprintf(log, LOG_INFO, "Client authenticated as %s\n", client_data->auth.user);
					client_send(log, client, "235 Authenticated\r\n");
				
					#ifndef CMAIL_NO_TLS
					switch(listener_data->tls_mode){
						case TLS_ONLY:
							client_data->current_mail.protocol="sesmtpa";
							break;
						case TLS_NEGOTIATE:
							if(client_data->tls_mode==TLS_ONLY){
								client_data->current_mail.protocol="esmtpsa";
								break;
							}
						case TLS_NONE:
							client_data->current_mail.protocol="esmtpa";
							break;
					}
					#else
					client_data->current_mail.protocol="esmtpa";
					#endif
					break;
			}
			client_data->state=STATE_IDLE;
			break;
	}
	
	return 0;
}

int smtpstate_idle(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	LISTENER* listener_data=(LISTENER*)client_data->listener->aux_data;

	if(!strncasecmp(client_data->recv_buffer, "noop", 4)){
		logprintf(log, LOG_INFO, "Client noop\n");
		client_send(log, client, "250 OK\r\n");
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "rset", 4)){
		logprintf(log, LOG_INFO, "Client reset\n");
		mail_reset(&(client_data->current_mail));
		client_send(log, client, "250 Reset OK\r\n");
		return 0;
	}

	#ifndef CMAIL_NO_TLS
	//accept only when offered, reject when already negotiated
	if(!strncasecmp(client_data->recv_buffer, "starttls", 8)){
		if(client_data->tls_mode!=TLS_NONE){
			logprintf(log, LOG_WARNING, "Client with active TLS session tried to negotiate\n");
			client_send(log, client, "503 Already in TLS session\r\n");
			return 0;
		}

		if(listener_data->tls_mode!=TLS_NEGOTIATE){
			logprintf(log, LOG_WARNING, "Client tried to negotiate TLS with non-negotiable listener\n");
			client_send(log, client, "503 Not advertised\r\n");
			return 0;
		}

		logprintf(log, LOG_INFO, "Client wants to negotiate TLS\n");
		mail_reset(&(client_data->current_mail));
		client_data->state=STATE_NEW;
		
		client_send(log, client, "220 Go ahead\r\n");
		
		client_data->tls_mode=TLS_NEGOTIATE;
		return tls_initclient(log, client->fd, client_data);
	}
	#endif

	if(!strncasecmp(client_data->recv_buffer, "auth ", 5)){
		switch(listener_data->auth_offer){
			case AUTH_NONE:
				logprintf(log, LOG_WARNING, "Client tried to auth on a non-auth listener\n");
				client_send(log, client, "503 Not advertised\r\n");
				return 0;
			case AUTH_TLSONLY:
				#ifndef CMAIL_NO_TLS
				if(client_data->tls_mode!=TLS_ONLY){
				#endif
					logprintf(log, LOG_WARNING, "Non-TLS client tried to auth on auth-tlsonly listener\n");
					client_send(log, client, "504 Encryption required\r\n"); //FIXME 538 might be better, but is market obsolete
					return 0;
				#ifndef CMAIL_NO_TLS
				}
				#endif
			case AUTH_ANY:
				if(client_data->auth.user){
					logprintf(log, LOG_WARNING, "Authenticated client tried to authenticate again\n");
					client_send(log, client, "503 Already authenticated\r\n");
					return 0;
				}
				//continue	
				break;
		}

		client_data->state=STATE_AUTH;
		return smtpstate_auth(log, client, database, path_pool);
	}

	if(!strncasecmp(client_data->recv_buffer, "xyzzy", 5)){
		client_send(log, client, "250 Nothing happens\r\n");
		logprintf(log, LOG_INFO, "Client performs incantation\n");
		//Using this command for some debug output...
		logprintf(log, LOG_DEBUG, "Peer name %s, mail submitter %s, data_allocated %d\n", client_data->peer_name, client_data->current_mail.submitter, client_data->current_mail.data_allocated);
		logprintf(log, LOG_DEBUG, "Authentication: %s\n", client_data->auth.user?client_data->auth.user:"none");
		#ifndef CMAIL_NO_TLS
		logprintf(log, LOG_DEBUG, "TLS State: %s\n", (client_data->tls_mode==TLS_NONE)?"none":"ok");
		#endif
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		logprintf(log, LOG_INFO, "Client quit\n");
		client_send(log, client, "221 OK Bye\r\n");
		return client_close(client);
	}

	//this needs to be implemented as per RFC5321 3.3
	if(!strncasecmp(client_data->recv_buffer, "rcpt ", 5)){
		logprintf(log, LOG_INFO, "Client tried to use RCPT in IDLE\n");
		client_send(log, client, "503 Bad sequence of commands\r\n");
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "mail from:", 10)){
		if(listener_data->auth_require && !client_data->auth.user){
			logprintf(log, LOG_WARNING, "Client tried mail command without authentication on strict auth listener\n");
			client_send(log, client, "530 Authentication required\r\n");
			return 0;
		}

		logprintf(log, LOG_INFO, "Client initiates mail transaction\n");
		//extract reverse path and store it
		if(path_parse(log, client_data->recv_buffer+10, &(client_data->current_mail.reverse_path))<0){
			client_send(log, client, "501 Path rejected\r\n");
			return -1;
		}

		//resolve reverse path
		//FIXME this does not resolve aliases
		switch(path_resolve(log, &(client_data->current_mail.reverse_path), database, false)){
			case 0:
				//either local or unknown
				//TODO filter local origin from unauthenticated connections
				break;
			case 1:
				//reject router  - if authed, router may still be any, so accept
				if(!client_data->auth.user){
					logprintf(log, LOG_DEBUG, "Originating mail rejected due to router setting\n");
					client_send(log, client, "550 User not allowed to originate mail\r\n");
					return 0;
				}
				break;
			default:
				//resolution failed
				logprintf(log, LOG_INFO, "Failed to resolve reverse path\n");
				client_send(log, client, "451 Path rejected\r\n");
				return -1;
		}

		if(client_data->auth.user){
			if(route_outbound(log, database, client_data->auth.user, &(client_data->current_mail.reverse_path))<0){
				//sending for this user/path combination prohibited
				client_send(log, client, "550 Authenticated user cannot use this path\r\n");
				return 0;
			}
		}

		//TODO call plugins for spf, etc

		client_send(log, client, "250 OK\r\n");
		client_data->state=STATE_RECIPIENTS;
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "vrfy ", 5)
			|| !strncasecmp(client_data->recv_buffer, "expn ", 5)){
		logprintf(log, LOG_WARNING, "Client tried to verify / expand an address, unsupported\n");
		client_send(log, client, "502 Not implemented\r\n");
		return 0;
	}

	logprintf(log, LOG_INFO, "Command not recognized in state IDLE: %s\n", client_data->recv_buffer);
	client_send(log, client, "500 Unknown command\r\n");
	return -1;
}

int smtpstate_recipients(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	unsigned i;
	MAILPATH* current_path;

	if(!strncasecmp(client_data->recv_buffer, "rcpt to:", 8)){
		//get slot in forward_paths
		for(i=0;i<SMTP_MAX_RECIPIENTS;i++){
			if(!client_data->current_mail.forward_paths[i]){
				break;
			}
		}

		if(i==SMTP_MAX_RECIPIENTS){
			//too many recipients, fail this one
			logprintf(log, LOG_INFO, "Mail exceeded recipient limit\n");
			client_send(log, client, "452 Too many recipients\r\n");
			return 0;
		}

		//get path from pool
		current_path=pathpool_get(log, path_pool);
		if(!current_path){
			logprintf(log, LOG_ERROR, "Failed to get path, failing recipient\n");
			client_send(log, client, "452 Recipients pool maxed out\r\n");
			//FIXME should state transition back to idle here?
			return 0;
		}

		if(path_parse(log, client_data->recv_buffer+8, current_path)<0){
			client_send(log, client, "501 Path rejected\r\n");
			pathpool_return(current_path);
			return -1;
		}

		switch(path_resolve(log, current_path, database, true)){
			case 0:
				//continue path handling
				break;
			case 1:
				//reject by router decision
				client_send(log, client, "551 User does not accept mail\r\n");
				pathpool_return(current_path);
				return 0;	
			default:
				client_send(log, client, "451 Path rejected\r\n");
				pathpool_return(current_path);
				//FIXME should state transition back to idle here?
				//client_data->state=STATE_IDLE;
				//mail_reset(&(client_data->current_mail));
				return 0;
		}

		if(!current_path->resolved_user){
			//path not local, accept only if authenticated
			if(!client_data->auth.user){
				client_send(log, client, "551 Unknown user\r\n");
				pathpool_return(current_path);
				return 0;
			}
		}

		//FIXME address deduplication?
		//TODO call plugins

		client_data->current_mail.forward_paths[i]=current_path;
		client_send(log, client, "250 Accepted\r\n");
		return 0;
	}
	
	if(!strncasecmp(client_data->recv_buffer, "quit", 4)){
		logprintf(log, LOG_INFO, "Client quit\n");
		client_send(log, client, "221 OK Bye\r\n");
		return client_close(client);
	}

	if(!strncasecmp(client_data->recv_buffer, "noop", 4)){
		logprintf(log, LOG_INFO, "Client noop\n");
		client_send(log, client, "250 OK\r\n");
		return 0;
	}
	
	if(!strncasecmp(client_data->recv_buffer, "rset", 4)){
		client_data->state=STATE_IDLE;
		mail_reset(&(client_data->current_mail));
		logprintf(log, LOG_INFO, "Client reset\n");
		client_send(log, client, "250 Reset OK\r\n");
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "auth", 4)){
		logprintf(log, LOG_INFO, "Client tried to use AUTH in RECIPIENTS\n");
		client_send(log, client, "503 Bad sequence of commands\r\n");
		return 0;
	}

	if(!strncasecmp(client_data->recv_buffer, "data", 4)){
		//reject command if no recipients
		if(!client_data->current_mail.forward_paths[0]){
			logprintf(log, LOG_INFO, "Data without recipients\n");
			client_send(log, client, "503 No valid recipients\r\n");
			return -1;
		}
		client_data->state=STATE_DATA;
		logprintf(log, LOG_INFO, "Client wants to begin data transmission\n");
		client_send(log, client, "354 Go ahead\r\n");
		return 0;
	}

	logprintf(log, LOG_INFO, "Command not recognized in state RECIPIENTS: %s\n", client_data->recv_buffer);
	client_send(log, client, "500 Unknown command\r\n");
	return -1;
}

int smtpstate_data(LOGGER log, CONNECTION* client, DATABASE* database, PATHPOOL* path_pool){
	CLIENT* client_data=(CLIENT*)client->aux_data;
	
	if(client_data->recv_buffer[0]=='.'){
		if(client_data->recv_buffer[1]){
			logprintf(log, LOG_INFO, "Data line with leading dot, fixing\n");
			//skip leading dot
			//FIXME use return value (might indicate message too long)
			return mail_line(log, &(client_data->current_mail), client_data->recv_buffer+1);
		}
		else{
			//end of mail
			logprintf(log, LOG_INFO, "End of mail data, routing\n");
			//TODO call plugins here

			if(!client_data->auth.user){
				switch(mail_route(log, &(client_data->current_mail), database)){
					case 250:
						logprintf(log, LOG_INFO, "Incoming mail accepted from %s\n", client_data->peer_name);
						client_send(log, client, "250 OK\r\n");
						break;
					default:
						logprintf(log, LOG_WARNING, "Mail not routed, rejecting\n");
						client_send(log, client, "554 Rejected\r\n");
						break;
				}
			}
			else{
				switch(mail_originate(log, client_data->auth.user, &(client_data->current_mail), database)){
					case 250:
						logprintf(log, LOG_INFO, "Originating mail accepted for user %s from %s\n", client_data->auth.user, client_data->peer_name);
						client_send(log, client, "250 OK\r\n");
						break;
					default:
						logprintf(log, LOG_WARNING, "Originated mail could not be routed, rejecting\n");
						client_send(log, client, "554 Rejected\r\n");
						break;
				}
			}
			mail_reset(&(client_data->current_mail));
			client_data->state=STATE_IDLE;
		}
	}
	else{
		//FIXME use return value (might indicate message too long)
		return mail_line(log, &(client_data->current_mail), client_data->recv_buffer);
	}

	return 0;
}
